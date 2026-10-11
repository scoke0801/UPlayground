"""Reproduce motion expansion inspection, migration, reload and runtime validation."""
import argparse
import json
import os
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step', choices=['inspect', 'audit', 'apply', 'validate', 'runtime', 'performance', 'promote'], required=True)
    parser.add_argument('--run')
    parser.add_argument('--render', action='store_true')
    parser.add_argument('--evidence', help='Successful runtime directory required for catalog promotion')
    parser.add_argument('--section', choices=['all','reaction','retreat'], default='all', help='Targeted runtime diagnosis; only all can promote')
    args = parser.parse_args()
    out = Path(args.run).resolve() if args.run else ROOT/'Saved/QA'/('MotionExpansion_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f'))
    out.mkdir(parents=True, exist_ok=True)
    if args.step in ('apply','promote') and (out/'transaction.json').exists():
        parser.error('Choose a fresh transaction directory; an existing backup must never be overwritten or rolled back by a new run')
    if args.step=='performance':
        from RunToonPerformance import gpu_sample, processes
        (out/'environment.json').write_text(json.dumps(dict(gpu=gpu_sample(),other_unreal_processes=processes()),indent=2),encoding='utf-8')
    env = dict(os.environ, PG_MOTION_RUN=str(out), PG_MOTION_STEP=args.step, PG_MOTION_SECTION=args.section)
    if args.step=='promote' and not args.evidence:parser.error('--promote requires --evidence')
    if args.evidence:env['PG_MOTION_EVIDENCE']=str(Path(args.evidence).resolve())
    version = json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation']
    engine = Path('C:/Program Files/Epic Games')/('UE_'+version)/'Engine/Binaries/Win64'
    runtime = args.step in ('runtime','performance')
    if args.step=='performance' and not args.render:parser.error('performance requires --render')
    script = ROOT/'Tools/Validation'/('ProbeMotionExpansionPerformance.py' if args.step=='performance' else 'ProbeMotionExpansion.py' if runtime else 'AuditMotionExpansion.py' if args.step=='audit' else 'ConfigureMotionExpansion.py')
    command = [str(engine/('UnrealEditor.exe' if runtime else 'UnrealEditor-Cmd.exe')), str(ROOT/'UPlayground.uproject'),
        '-unattended', '-nosound', '-nop4', '-culture=en', '-DisablePlugins=RiderLink', '-EnablePlugins=PythonScriptPlugin',
        '-ddc=InstalledNoZenLocalFallback', '-PGRunSeed=173001', '-PGTestProfile='+out.name, '-UserDir='+str(out/'User'),
        '-abslog='+str(out/(args.step+'.log'))]
    command += (['-ExecutePythonScript='+str(script)] if runtime else ['-run=pythonscript', '-script='+str(script)])
    command += ['-RenderOffscreen', '-windowed', '-ResX=1280', '-ResY=720'] if args.render else ['-nullrhi']
    print(out, flush=True)
    with (out/(args.step+'.stdout.log')).open('w', encoding='utf-8') as stream:
        process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=stream, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            code = process.wait(timeout=900)
        except subprocess.TimeoutExpired:
            process.kill(); process.wait(); code = -1
    log = (out/(args.step+'.log')).read_text(encoding='utf-8', errors='replace')
    passed = code == 0 and 'PGMotionExpansion PASS' in log and not any(x in log for x in ['LogPython: Error', 'Fatal error:', 'Assertion failed:'])
    report = dict(step=args.step, status='PASS' if passed else 'FAIL', code=code, command=command)
    if not passed and args.step in ('apply','promote') and (out/'transaction.json').exists():
        from PlayableCharacterTransaction import restore
        restore(ROOT, out)
        report['rollback'] = 'RESTORED'
    (out/(args.step+'-result.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(report['status'], code, flush=True)
    return 0 if passed else 1

if __name__ == '__main__':
    raise SystemExit(main())
