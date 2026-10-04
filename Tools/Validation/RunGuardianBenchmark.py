"""Run the native 5/50/5 guardian CPU fixture outside PIE, optionally in a cooked package."""
import argparse
from datetime import datetime, timezone
import json
import os
import platform
from pathlib import Path
import re
import shutil
import uuid
from RunQA import ROOT, FATAL, run_process, read_text, unexpected_errors
from RunGuardianSoak import summarize_csv


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=1200)
    parser.add_argument('--exe', type=Path, help='Cooked UPlayground/Binaries/Win64/UPlayground.exe')
    parser.add_argument('--trace', action='store_true')
    parser.add_argument('--engine-setting', action='append', default=[], metavar='NAME=VALUE',
                        help='Diagnostic SystemSettings override; repeat for independent settings')
    parser.add_argument('--fps-cap', type=int, default=60, help='Use 0 to measure capacity without frame-limiter sleep')
    args = parser.parse_args()
    if not 120 <= args.seconds <= 3600:
        parser.error('--seconds must be 120..3600')
    if not 0 <= args.fps_cap <= 240:
        parser.error('--fps-cap must be 0..240')
    for setting in args.engine_setting:
        if not re.fullmatch(r'[A-Za-z0-9_.]+=-?[0-9.]+', setting):
            parser.error('--engine-setting expects a numeric NAME=VALUE')
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_')+uuid.uuid4().hex[:8]+'_guardian_native'
    out = ROOT/'Saved/QA'/run_id
    out.mkdir(parents=True)
    version = json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'Epic Games'/f'UE_{version}'
    command = ([args.exe.resolve()] if args.exe else
        [engine/'Engine/Binaries/Win64/UnrealEditor.exe', ROOT/'UPlayground.uproject', '-game'])
    command += ['/Game/Maps/RogueArena', '-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720',
        '-nosplash', '-unattended', '-nop4', '-culture=en', '-UTF8Output', '-csvGpuStats',
        '-DisablePlugins=RiderLink', '-ddc=InstalledNoZenLocalFallback',
        '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0',
        f'-PGTestProfile=GuardianSoak_{run_id}', '-PGRunSeed=173001', f'-PGGuardianBenchmarkId={run_id}',
        f'-PGGuardianBenchmarkFPS={args.fps_cap}',
        f'-ExecCmds=PGGuardianBenchmark {args.seconds}', f'-abslog={out/"benchmark.log"}',
        '-TestExit=PGGuardianBenchmark COMPLETE+PGGuardianBenchmark FAILED']
    if args.trace:
        command += ['-trace=cpu,frame,bookmark', f'-tracefile={out/"cpu.utrace"}', '-statnamedevents']
    for setting in args.engine_setting:
        command.append('-ini:Engine:[SystemSettings]:'+setting)
    print(out, flush=True)
    code, timed_out = run_process(command, ROOT, out/'stdout.log', args.seconds+240)
    log = read_text(out/'benchmark.log') if (out/'benchmark.log').exists() else ''
    errors = unexpected_errors(log, 'guardian_native')
    if code or timed_out or FATAL.search(log):
        errors.append(f'Process failed: code={code}, timeout={timed_out}')
    if not re.search(r'PGGuardianBenchmark COMPLETE enemies=0 weapons=\d+ assisted=1 direct_input=0', log):
        errors.append('Missing native benchmark completion/cleanup')
    durations = [args.seconds//4, args.seconds//2, args.seconds-args.seconds//4-args.seconds//2]
    phases = re.findall(r'PGGuardianBenchmark phase=(\w+) duration=([\d.]+) game_seconds=([\d.]+) windups=(\d+) recoveries=(\d+)', log)
    if [p[0] for p in phases] != ['baseline', 'dense', 'return']:
        errors.append('Incomplete native benchmark phases')
    for phase, duration in zip(phases, durations):
        if float(phase[1]) < duration or float(phase[2]) < .9*duration or min(int(phase[3]), int(phase[4])) <= 0:
            errors.append(phase[0]+': insufficient elapsed time/AI activity')
    metrics = {}
    warnings = []
    # Editor -game uses the project Saved folder; cooked games log their resolved CSV path.
    saved_roots = [ROOT/'Saved']
    if args.exe:
        saved_roots += [args.exe.resolve().parents[2]/'Saved', Path(os.environ['LOCALAPPDATA'])/'UPlayground/Saved']
    for name in ('baseline', 'dense', 'return'):
        filename = run_id+'_'+name+'.csv'
        candidates = [p/'Profiling/CSV'/filename for p in saved_roots]
        source = next((p for p in candidates if p.exists()), None)
        if source is None:
            errors.append(name+': CSV missing')
            continue
        shutil.copy2(source, out/(name+'.csv'))
        metrics[name] = summarize_csv(out/(name+'.csv'))
        if metrics[name]['frames'] < 60 or any(key not in metrics[name]['metrics']
            for key in ('FrameTime', 'GameThreadTime', 'GPUTime')):
            errors.append(name+': incomplete timing')
        elif metrics[name]['metrics']['GPUTime']['mean'] <= 0:
            errors.append(name+': no GPU samples')
        if metrics[name]['metrics'].get('FrameTime', {}).get('p95', 0) > 16.7:
            warnings.append(name+': frame p95 exceeds proposed 16.7 ms target')
        if metrics[name]['metrics'].get('FrameTime', {}).get('p99', 0) > 33.3:
            warnings.append(name+': frame p99 exceeds 33.3 ms')
        if metrics[name]['metrics'].get('FrameTime', {}).get('maximum', 0) > 100:
            warnings.append(name+': frame hitch exceeds 100 ms; inspect CPU/PSO trace')
    if args.seconds < 1200:
        warnings.append('Short diagnostic; not a 20-minute stability result')
    report = dict(status='FAIL' if errors else 'PASS_WITH_WARNINGS' if warnings else 'PASS',
        errors=errors, warnings=warnings, metrics=metrics, phases=phases, packaged=bool(args.exe),
        seconds=args.seconds, fps_cap=args.fps_cap, assisted=True, direct_input=False, command=[str(arg) for arg in command],
        machine=dict(os=platform.platform(), cpu=os.environ.get('PROCESSOR_IDENTIFIER'), logical_cpus=os.cpu_count()),
        limits=['Static player, scripted repositioning, real AI/GAS; not direct-input or balance QA',
                'Performance fixture does not validate free navigation, full-run completion or audio quality'])
    (out/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(dict(status=report['status'], errors=errors, warnings=warnings, out=str(out))), flush=True)
    return int(bool(errors))


if __name__ == '__main__':
    raise SystemExit(main())
