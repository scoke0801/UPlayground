"""Sequential GPU ablation with concurrent Unreal-process detection and raw evidence.

Uses UE's bundled Python. Never closes an existing editor or changes project settings.
"""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
import re
from pathlib import Path
import shutil
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]


def processes():
    result = subprocess.run(['powershell', '-NoProfile', '-Command',
        'Get-Process | Select-Object ProcessName,Id | ConvertTo-Json -Compress'],
        capture_output=True, text=True, timeout=15, check=True)
    rows = json.loads(result.stdout)
    return [{'name': row['ProcessName'], 'pid': row['Id']} for row in rows
            if row['ProcessName'].lower().startswith(('unrealeditor', 'shadercompileworker', 'uplayground'))]


def gpu_sample():
    try:
        result = subprocess.run(['nvidia-smi', '--query-gpu=name,driver_version,utilization.gpu,memory.used,temperature.gpu,clocks.sm,power.draw',
                                 '--format=csv,noheader,nounits'], capture_output=True, text=True, timeout=10, check=True)
        return result.stdout.strip()
    except (OSError, subprocess.SubprocessError) as error:
        return 'unavailable: '+str(error)


def csv_complete(path):
    """UE writes capture metadata last; STOP alone does not flush its async writer."""
    try:
        with Path(path).open('rb') as stream:
            stream.seek(0, 2)
            stream.seek(max(0, stream.tell()-65536))
            lines = stream.read().decode('utf-8-sig').splitlines()
        row = next(csv.reader([lines[-1]]))
        if len(row) % 2 or row[0] != '[HasHeaderRowAtEnd]':
            return False
        footer = dict(zip(row[::2], row[1::2]))
        duration = float(footer['[captureduration]'])
        return (footer['[HasHeaderRowAtEnd]'] == '1'
                and float(footer['[endtimestamp]']) > 0
                and math.isfinite(duration) and duration > 0
                and footer['[csvmaxfilebytesreached]'] == '0')
    except (OSError, UnicodeError, csv.Error, IndexError, KeyError, ValueError, StopIteration):
        return False


def metrics(path):
    values = {}
    csv.field_size_limit(10_000_000)
    with Path(path).open(encoding='utf-8-sig') as stream:
        for row in csv.DictReader(stream):
            try:
                frame = float(row['FrameTime'])
                if not math.isfinite(frame) or frame <= 0:
                    continue
            except (ValueError, TypeError, KeyError):
                continue
            for key, raw in row.items():
                if key not in ['FrameTime', 'GameThreadTime', 'RenderThreadTime', 'GPUTime', 'GPUMem/LocalUsedMB'] and not str(key).startswith('GPU/'):
                    continue
                try:
                    value = float(raw)
                    if math.isfinite(value):
                        values.setdefault(key, []).append(value)
                except (ValueError, TypeError):
                    pass
    result = {key: dict(samples=len(v), mean=statistics.mean(v), median=statistics.median(v),
                      p95=sorted(v)[math.ceil(len(v)*.95)-1],
                      p99=sorted(v)[math.ceil(len(v)*.99)-1], maximum=max(v)) for key, v in values.items()}
    frames = values.get('FrameTime', [])
    if frames:
        result['FrameTime']['budget_exceedances'] = [
            dict(threshold_ms=threshold, count=sum(v > threshold for v in frames),
                 percent=100*sum(v > threshold for v in frames)/len(frames))
            for threshold in (1000/60, 1000/30, 50, 100)]
    return result


def summarize(out, runtime, monitor, code, timed_out):
    report = dict(status='CHARACTERIZED', phases=[], errors=[], warnings=[], comparisons={},
                  viewport_size=runtime.get('viewport_size'), settings=runtime.get('settings'),
                  limits=['Editor PIE animated Inori art fixture, per-phase forced LOD; no combat/AI or packaged certification',
                          'Process snapshots detect concurrent Unreal/game/compiler processes; unrelated desktop GPU work is not excluded',
                          'Ablations remove features for diagnosis, not production quality presets',
                          'Pass timings can overlap; marginal differences are not additive'])
    if code or timed_out or runtime.get('status') != 'PASS':
        report['errors'].append(f"Execution failed: exit={code}, timeout={timed_out}, runtime={runtime.get('error', runtime.get('status'))}")
    # Shader workers during startup are expected; reject them during measurement.
    intervals = [(p['capture_start'], p['capture_end']) for p in runtime.get('phases', []) if 'capture_start' in p]
    relevant = [s for s in monitor if not intervals or any(a-4 <= s.get('monotonic', 0) <= b+4 for a, b in intervals)]
    contaminated = [s for s in relevant if s.get('competitors') or s.get('process_error')]
    if not relevant:
        report['errors'].append('Missing process monitoring during samples')
    if contaminated:
        report['errors'].append('Concurrent Unreal/game/compiler process or unavailable process audit; reject timing attribution')
    by_name = {}
    expected = [p['name'] for p in runtime.get('phase_plan', [])]
    actual = [p['name'] for p in runtime.get('phases', [])]
    if not expected or actual != expected:
        report['errors'].append('Incomplete or reordered phase sequence')
    motion = runtime.get('motion_checks', [])
    if [m['phase'] for m in motion] != expected or not all(m.get('bone_motion') for m in motion):
        report['errors'].append('Missing animation clock/bone-motion verification for each phase')
    for phase in runtime.get('phases', []):
        try:
            source = Path(phase['csv'])
            shutil.copy2(source, out / source.name)
            if not csv_complete(source):
                raise ValueError('CSV is incomplete: missing final capture metadata or reached file size limit')
            measured = metrics(source)
            for key in ['FrameTime', 'GPUTime', 'GameThreadTime', 'RenderThreadTime']:
                if measured.get(key, {}).get('samples', 0) < 120 or measured[key]['mean'] <= 0:
                    raise ValueError('Insufficient valid samples for '+key)
            by_name[phase['name']] = measured
            report['phases'].append({**phase, 'metrics': measured})
        except (OSError, ValueError) as error:
            report['errors'].append(phase['name']+': '+str(error))
    full = [v['GPUTime']['mean'] for k, v in by_name.items() if k.startswith('full_')]
    stable = len(full) >= 3 and max(full)/min(full) <= 1.15
    report['full_repeat_gpu_ratio'] = max(full)/min(full) if full else None
    if not stable:
        report['warnings'].append('Identical 50-character repeats differ by >15% or are incomplete')
    for first, repeat in [('lod1', 'lod1_repeat'), ('lod2', 'lod2_repeat')]:
        if first in by_name and repeat in by_name:
            pair = [by_name[k]['GPUTime']['mean'] for k in [first, repeat]]
            report[first+'_repeat_gpu_ratio'] = max(pair)/min(pair)
            if max(pair)/min(pair) > 1.15:
                stable = False
                report['warnings'].append(first+': repeated candidate GPU differs by >15%')
    if 'one' in by_name and 'one_return' in by_name:
        one = [by_name[n]['GPUTime']['mean'] for n in ['one', 'one_return']]
        report['one_return_gpu_ratio'] = max(one)/min(one)
        if max(one)/min(one) > 1.15:
            stable = False
            report['warnings'].append('1-character return drift exceeds 15%')
    report['status'] = 'FAIL' if report['errors'] else 'CHARACTERIZED' if stable else 'UNSTABLE_PERFORMANCE_SAMPLE'
    for name, before, after in [('no_post', 'full_a', 'full_b'), ('no_outline', 'full_b', 'full_c'),
                                ('no_transparency', 'full_c', 'full_d'), ('no_shadows', 'full_d', 'full_e'),
                                ('lod1', 'full_a', 'full_b'), ('lod1_repeat', 'full_b', 'full_c'),
                                ('lod2', 'full_c', 'full_d'), ('lod2_repeat', 'full_d', 'full_e')]:
        if name not in by_name:
            continue
        delta = None
        baseline_range = None
        if report['status'] == 'CHARACTERIZED' and before in by_name and after in by_name:
            endpoints = [by_name[k]['GPUTime']['mean'] for k in [before, after]]
            baseline = statistics.mean(endpoints)
            delta = baseline-by_name[name]['GPUTime']['mean']
            baseline_range = [min(endpoints), max(endpoints)]
        report['comparisons'][name] = {'bracketed_gpu_saving_ms': delta,
                                     'baseline_endpoint_range_ms': baseline_range}
    for name, value in by_name.items():
        if value['FrameTime']['p95'] > 16.7:
            report['warnings'].append(name+': frame p95 exceeds 16.7 ms')
    if by_name and all(v['RenderThreadTime']['mean'] < .1 for v in by_name.values()):
        report['warnings'].append('CSV RenderThreadTime is near zero; use CPU trace scopes, not this counter, to assess rendering CPU cost')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--warmup', type=float, default=8)
    parser.add_argument('--sample', type=float, default=12)
    parser.add_argument('--width', type=int, default=1280)
    parser.add_argument('--height', type=int, default=720)
    parser.add_argument('--lod-candidate', action='store_true')
    parser.add_argument('--fixed-lod', type=int, choices=(0, 1, 2),
                        help='Use the candidate mesh at this LOD for every ablation')
    parser.add_argument('--repeats-only', action='store_true', help='Three identical 50-character phases for CPU diagnosis')
    parser.add_argument('--trace', action='store_true', help='Record CPU/frame trace with per-capture named regions')
    args = parser.parse_args()
    if args.lod_candidate and (args.fixed_lod is not None or args.repeats_only):
        parser.error('--lod-candidate cannot be combined with --fixed-lod or --repeats-only')
    if not all(math.isfinite(v) for v in (args.warmup, args.sample)) or args.warmup < 5 or args.sample < 10 or not (64 <= args.width <= 4096 and 64 <= args.height <= 4096):
        parser.error('warmup >=5, sample >=10, dimensions 64..4096 required')
    run = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    out = ROOT/'Saved/ToonTest/Performance'/run
    out.mkdir(parents=True)
    print(out, flush=True)
    initial = processes()
    if initial:
        (out/'report.json').write_text(json.dumps(dict(status='BLOCKED_CONCURRENT_PROCESS', processes=initial), indent=2))
        print('Existing Unreal/game/compiler process; no process closed: '+str(initial), flush=True)
        return 2
    version = json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'Epic Games'/('UE_'+version)
    phase_count = 3 if args.repeats_only else 9 if args.lod_candidate else 12
    options = vars(args) | {'timeout': 180+phase_count*(args.warmup+args.sample)}
    (out/'options.json').write_text(json.dumps(options, indent=2))
    command = [str(engine/'Engine/Binaries/Win64/UnrealEditor.exe'), str(ROOT/'UPlayground.uproject'),
               '-RenderOffscreen', '-windowed', '-ForceRes', '-ResX='+str(args.width), '-ResY='+str(args.height),
               '-unattended', '-NoSound', '-NoSplash', '-nop4', '-culture=en', '-UTF8Output', '-csvGpuStats',
               '-DisablePlugins=RiderLink', '-EnablePlugins=PythonScriptPlugin', '-ddc=InstalledNoZenLocalFallback',
               '-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeToonPerformance.py'), '-abslog='+str(out/'editor.log')]
    if args.trace:
        command += ['-trace=cpu,frame,bookmark,region', '-tracefile='+str(out/'cpu.utrace'), '-statnamedevents']
    (out/'command.json').write_text(json.dumps(command, indent=2))
    sources = ['Tools/Validation/RunToonPerformance.py', 'Tools/Validation/ProbeToonPerformance.py',
               'Config/DefaultEngine.ini', 'Binaries/Win64/UnrealEditor-PGActor.dll',
               'Saved/ToonTest/lighting_lab.json', 'Saved/ToonTest/retarget.json',
               'Content/Art/ToonTest/Inori/SK_Inori_ToonTest.uasset']
    if args.lod_candidate or args.fixed_lod is not None:
        sources += ['Content/Art/ToonTest/Advanced/Performance/SK_Inori_ToonLOD.uasset',
                    'Saved/ToonTest/performance_lod.json']
    (out/'source_hashes.json').write_text(json.dumps({p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources}, indent=2))
    monitor = [dict(seconds=0, competitors=initial, gpu=gpu_sample())]
    started = time.monotonic()
    timed_out = False
    with (out/'stdout.log').open('w', encoding='utf-8') as log:
        process = subprocess.Popen(command, cwd=ROOT, env=dict(os.environ, PG_TOON_PERF_OUT=str(out)),
                                   stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
        while process.poll() is None:
            sample = dict(seconds=time.monotonic()-started, monotonic=time.monotonic(), gpu=gpu_sample())
            try:
                sample['competitors'] = [p for p in processes() if p['pid'] != process.pid]
            except (OSError, subprocess.SubprocessError) as error:
                sample['process_error'] = str(error)
            monitor.append(sample)
            (out/'monitor.json').write_text(json.dumps(monitor, indent=2))
            if time.monotonic()-started > options['timeout']:
                timed_out = True
                process.kill()  # Only the child launched by this invocation.
                process.wait(timeout=30)
                break
            time.sleep(3)
    runtime = json.loads((out/'runtime.json').read_text()) if (out/'runtime.json').exists() else {}
    editor_log = (out/'editor.log').read_text(encoding='utf-8-sig', errors='replace') if (out/'editor.log').exists() else ''
    failures = re.findall(r'^.*(?:Fatal error:|LogPython: Error:|Failed to compile Material|ShaderCompileWorker crashed).*$'
                          , editor_log, flags=re.MULTILINE)
    if failures:
        runtime.update(status='FAIL', error='\n'.join(failures[-10:]))
    report = summarize(out, runtime, monitor, process.returncode, timed_out)
    report['run'] = str(out)
    report['process_id'] = process.pid
    report['options'] = options
    if args.trace:
        report['warnings'].append('CPU tracing adds overhead; compare against an untraced run before attributing timing changes')
        if not (out/'cpu.utrace').is_file() or (out/'cpu.utrace').stat().st_size < 1024:
            report['errors'].append('Requested CPU trace is missing or empty')
            report['status'] = 'FAIL'
    (out/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    (ROOT/'Saved/ToonTest/performance_latest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'phases'}, indent=2), flush=True)
    for phase in report['phases']:
        print(phase['name'], 'GPU', round(phase['metrics']['GPUTime']['mean'], 3),
              'frame p95', round(phase['metrics']['FrameTime']['p95'], 3), flush=True)
    return int(report['status'] != 'CHARACTERIZED')


if __name__ == '__main__':
    raise SystemExit(main())
