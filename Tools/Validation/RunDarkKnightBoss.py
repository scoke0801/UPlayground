"""Isolated Dark Knight authoring and verification commands."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--step', choices=['inspect', 'grip', 'apply', 'roster', 'validate', 'preview', 'runtime'], required=True)
    parser.add_argument('--stage-roster', action='store_true', help='Test saved stage spawns without encounter overrides')
    parser.add_argument('--run')
    parser.add_argument('--reuse-motions', action='store_true')
    args = parser.parse_args()
    out = Path(args.run).resolve() if args.run else ROOT/'Saved/DarkKnightBoss'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')
    out.mkdir(parents=True, exist_ok=True)
    os.environ['PG_DARK_KNIGHT_RUN'] = str(out)
    version = json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation']
    engine = Path('C:/Program Files/Epic Games')/('UE_'+version)/'Engine/Binaries/Win64'
    render = args.step in ('preview', 'runtime')
    script = {'inspect':'InspectDarkKnightBoss.py', 'grip':'InspectDarkKnightGrip.py', 'apply':'ConfigureDarkKnightBoss.py',
              'validate':'ConfigureDarkKnightBoss.py', 'roster':'ConfigureDarkKnightBoss.py', 'preview':'PreviewDarkKnightBoss.py',
              'runtime':'ProbeDarkKnightBoss.py'}[args.step]
    command = [str(engine/('UnrealEditor.exe' if render else 'UnrealEditor-Cmd.exe')),
        str(ROOT/'UPlayground.uproject'), '-unattended', '-nop4', '-nosound', '-culture=ko',
        '-DisablePlugins=RiderLink', '-EnablePlugins=PythonScriptPlugin', '-ddc=InstalledNoZenLocalFallback',
        '-UserDir='+str(out/'User'), '-PGTestProfile=DarkKnight_'+out.name,
        '-abslog='+str(out/(args.step+'.log'))]
    command += (['-RenderOffscreen', '-windowed', '-ResX=1280', '-ResY=720',
                 '-ExecutePythonScript='+str(ROOT/'Tools/Validation'/script)] if render else
                ['-nullrhi', '-run=pythonscript', '-script='+str(ROOT/'Tools/Validation'/script)])
    if args.step == 'validate': command.append('-PGDarkKnightValidate')
    if args.step == 'roster': command.append('-PGDarkKnightRoster')
    if args.stage_roster: command.append('-PGDarkKnightStageRoster')
    if args.reuse_motions: command.append('-PGDarkKnightReuseMotions')
    print(out, flush=True)
    with (out/(args.step+'.stdout.log')).open('w', encoding='utf-8') as stream:
        process = subprocess.Popen(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        try: code = process.wait(timeout=900)
        except subprocess.TimeoutExpired:
            process.kill(); process.wait(); raise
    log = (out/(args.step+'.log')).read_text(encoding='utf-8-sig', errors='replace')
    passed = code == 0 and 'PGDarkKnight '+args.step+' PASS' in log and not any(x in log for x in ('LogPython: Error', 'Fatal error:', 'Ensure condition failed'))
    result = dict(step=args.step, status='PASS' if passed else 'FAIL', exit_code=code)
    (out/(args.step+'-result.json')).write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(result, flush=True)
    return not passed


if __name__ == '__main__': raise SystemExit(main())
