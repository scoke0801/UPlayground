"""20-minute rendered guardian AI soak; a short --seconds run is only a smoke test."""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
import platform
from pathlib import Path
import shutil
import statistics
import subprocess
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, unexpected_errors
from AnalyzeGuardianAudio import analyze


def content_hashes():
    paths = list((ROOT/'Content/DataCenter/Guardian').glob('*.uasset'))
    paths += list((ROOT/'Content/Art/Guardian').glob('*.uasset'))
    paths += list((ROOT/'Content/DataCenter/DataTables').rglob('*.uasset'))
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(paths)}


def summarize_csv(path):
    csv.field_size_limit(10_000_000)
    values = {}
    frames = 0
    with path.open(encoding='utf-8-sig') as stream:
        for row in csv.DictReader(stream):
            try:
                frame = float(row['FrameTime'])
            except (KeyError, TypeError, ValueError):
                continue
            if not math.isfinite(frame) or frame <= 0:
                continue
            frames += 1
            for key in ('FrameTime', 'GameThreadTime', 'RenderThreadTime', 'GPUTime', 'Memory/PhysicalUsedMB',
                        'GPUMem/LocalUsedMB', 'GPUMem/LocalBudgetMB'):
                try:
                    value = float(row.get(key, row.get('PhysicalUsedMB')) if key == 'Memory/PhysicalUsedMB' else row[key])
                    if math.isfinite(value):
                        values.setdefault(key, []).append(value)
                except (KeyError, TypeError, ValueError):
                    pass
    metrics = {}
    for key, items in values.items():
        ordered = sorted(items)
        metrics[key] = dict(unit='MB' if key.endswith('MB') else 'ms',
            mean=round(statistics.mean(items), 3), p95=round(ordered[math.ceil(.95*len(items))-1], 3),
            p99=round(ordered[math.ceil(.99*len(items))-1], 3), maximum=round(max(items), 3),
            first_300_mean=round(statistics.mean(items[:300]), 3), last_300_mean=round(statistics.mean(items[-300:]), 3))
    return dict(frames=frames, metrics=metrics)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=1200)
    args = parser.parse_args()
    if args.seconds < 120:
        parser.error('--seconds must be at least 120 (three audio/CSV phases)')
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_')+uuid.uuid4().hex[:8]+'_guardian_soak'
    out = ROOT/'Saved/QA'/run_id
    out.mkdir(parents=True)
    phases = [dict(name='baseline', packs=1, seconds=args.seconds//4),
              dict(name='dense', packs=10, seconds=args.seconds//2),
              dict(name='return', packs=1, seconds=args.seconds-args.seconds//4-args.seconds//2)]
    config = dict(run_id=run_id, out=str(out), warmup_seconds=30 if args.seconds >= 1200 else 5,
                  capture_images=args.seconds < 1200, phases=phases)
    (out/'config.json').write_text(json.dumps(config, indent=2), encoding='utf-8')
    version = json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'Epic Games'/f'UE_{version}'
    command = [engine/'Engine/Binaries/Win64/UnrealEditor.exe', ROOT/'UPlayground.uproject',
        '-EnablePlugins=PythonScriptPlugin', f'-ExecutePythonScript={ROOT/"Tools/Validation/ProbeGuardianSoakPIE.py"}',
        '-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720', '-nosplash', '-unattended',
        '-nop4', '-culture=en', '-UTF8Output', '-csvGpuStats', '-PGRunSeed=173001', '-DisablePlugins=RiderLink',
        '-ddc=InstalledNoZenLocalFallback', '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0',
        f'-PGTestProfile=GuardianSoak_{run_id}', f'-PGGuardianSoakConfig={out/"config.json"}', f'-abslog={out/"soak.log"}']
    print(str(out), flush=True)
    before_hashes = content_hashes()
    try:
        gpu = subprocess.run(['nvidia-smi', '--query-gpu=name,driver_version,memory.total', '--format=csv,noheader'],
            capture_output=True, text=True, timeout=10).stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        gpu = 'unavailable'
    code, timeout = run_process(command, ROOT, out/'stdout.log', args.seconds+240)
    log = read_text(out/'soak.log') if (out/'soak.log').exists() else ''
    errors = unexpected_errors(log, 'guardian_soak')
    if content_hashes() != before_hashes:
        errors.append('Authored content changed during measurement')
    if code or timeout or FATAL.search(log):
        errors.append(f'Process failed: code={code}, timeout={timeout}')
    observed = json.loads(read_text(out/'observations.json')) if (out/'observations.json').exists() else {}
    if observed.get('status') != 'PASS' or observed.get('remaining_tagged_enemies') != 0:
        errors.append('Missing passing observations/cleanup')
    if [stage.get('name') for stage in observed.get('stages', [])] != [phase['name'] for phase in phases]:
        errors.append('Incomplete phase sequence')
    for stage, phase in zip(observed.get('stages', []), phases):
        if stage.get('duration_observed', 0) < phase['seconds']:
            errors.append(phase['name']+': completed too early')
        if any(stage.get(key, 0) <= 0 for key in ('windup_entries', 'recovery_entries')):
            errors.append(phase['name']+': missing AI activity')
    metrics = {}
    warnings = []
    for phase in phases:
        name = phase['name']
        source = ROOT/'Saved/Profiling/CSV'/(run_id+'_'+name+'.csv')
        if source.exists():
            shutil.copy2(source, out/(name+'.csv'))
            metrics[name] = summarize_csv(out/(name+'.csv'))
            if metrics[name]['frames'] < 60:
                errors.append(name+': too few CSV frames')
            for key in ('FrameTime', 'GameThreadTime', 'RenderThreadTime', 'GPUTime', 'Memory/PhysicalUsedMB'):
                if key not in metrics[name]['metrics']:
                    errors.append(name+': missing '+key)
            if metrics[name]['metrics'].get('GPUTime', {}).get('mean', 0) <= 0:
                errors.append(name+': GPU timing not captured')
            if metrics[name]['metrics'].get('FrameTime', {}).get('p95', 0) > 16.7:
                warnings.append(name+': frame p95 exceeds proposed 16.7 ms target')
        else:
            errors.append(name+': CSV missing')
        for suffix in (('.wav', '.png') if config['capture_images'] else ('.wav',)):
            if not (out/(name+suffix)).exists():
                errors.append(name+suffix+' missing')
    if args.seconds < 1200:
        warnings.append('Short smoke test; does not satisfy 20-minute soak')
    try:
        audio = analyze(out)
        for capture in audio['captures']:
            if not 13 <= capture['duration_seconds'] <= 17:
                audio['errors'].append(capture['path']+': expected continuous 15-second master capture')
        if audio['errors']:
            audio['status'] = 'FAIL'
        (out/'audio.json').write_text(json.dumps(audio, indent=2), encoding='utf-8')
        errors.extend(audio['errors'])
        warnings.extend(audio['warnings'])
    except (OSError, ValueError) as error:
        errors.append('Audio analysis failed: '+str(error))
    report = dict(status='FAIL' if errors else ('PASS_WITH_WARNINGS' if warnings else 'PASS'),
        errors=errors, warnings=warnings, metrics=metrics, observations=observed,
        gpu=gpu, command=[str(arg) for arg in command],
        machine=dict(os=platform.platform(), cpu=os.environ.get('PROCESSOR_IDENTIFIER'), logical_cpus=os.cpu_count()),
        authored_content_sha256=before_hashes,
        limits=['Editor PIE, 1280x720, 60 fps cap; not a packaged performance result',
                'Health assistance and scripted repositioning; no direct-input or balance verdict',
                'Actor counts and process memory are observations, not a timer/subscription leak proof'])
    (out/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(dict(status=report['status'], errors=errors, warnings=warnings, out=str(out))), flush=True)
    return int(bool(errors))


if __name__ == '__main__':
    raise SystemExit(main())
