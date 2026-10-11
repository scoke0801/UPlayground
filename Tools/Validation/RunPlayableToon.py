"""Back up/apply/reload playable toon data, then exercise actual player appearance swaps in PIE."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
from RunPlayableCharacters import run_process, FATAL, read_text
from PlayableCharacterTransaction import restore, write_json

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step', choices=['all', 'configure', 'preview'], default='all')
    args = parser.parse_args()
    out = ROOT/'Saved/PlayableToon'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    out.mkdir(parents=True)
    os.environ['PG_PLAYABLE_TOON_RUN'] = str(out)
    version = json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation']
    engine = Path('C:/Program Files/Epic Games')/('UE_'+version)/'Engine/Binaries/Win64'
    report = dict(status='RUNNING', run=str(out), steps=[])
    print(out, flush=True)
    try:
        steps = ['configure', 'verify', 'preview'] if args.step == 'all' else ['configure', 'verify'] if args.step == 'configure' else ['preview']
        for step in steps:
            render = step == 'preview'
            script = 'PreviewPlayableToon.py' if render else 'ConfigurePlayableToon.py'
            cmd = [engine/('UnrealEditor.exe' if render else 'UnrealEditor-Cmd.exe'), ROOT/'UPlayground.uproject',
                   '-unattended', '-nosound', '-nop4', '-culture=en', '-DisablePlugins=RiderLink',
                   '-EnablePlugins=PythonScriptPlugin', '-Multiprocess', '-ddc=InstalledNoZenLocalFallback',
                   '-PGTestProfile=PlayableToon_'+out.name, '-abslog='+str(out/(step+'.log'))]
            cmd += ['-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720', '-ExecutePythonScript='+str(ROOT/'Tools/Validation'/script)] if render else ['-nullrhi', '-run=pythonscript', '-script='+str(ROOT/'Tools/Validation'/script)]
            if step == 'verify': cmd.append('-PGPlayableToonVerify')
            code, timeout = run_process(cmd, ROOT, out/(step+'_stdout.log'), 600)
            assert code == 0 and not timeout and not FATAL.search(read_text(out/(step+'.log'))), (step, code)
            result = json.loads((out/(step+'.json')).read_text(encoding='utf-8'))
            assert result['status'] == 'PASS', result
            report['steps'].append(dict(step=step, status='PASS'))
        report['status'] = 'PASS'
    except BaseException:
        report['status'] = 'FAIL'
        if (out/'transaction.json').exists() and not any(s['step'] == 'verify' for s in report['steps']):
            restore(ROOT, out)
            report['restored'] = True
        raise
    finally:
        write_json(out/'run.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
