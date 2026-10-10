"""Run map art scripts with the installed editor, without compiling or packaging."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from datetime import datetime

ROOT = Path(__file__).resolve().parents[3]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('script')
    parser.add_argument('--render', action='store_true')
    args = parser.parse_args()
    version = json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation']
    engine = Path('C:/Program Files/Epic Games')/('UE_'+version)/'Engine/Binaries/Win64'
    run = ROOT/'Saved/LevelDressing'/datetime.now().strftime('%Y%m%d_%H%M%S')
    run.mkdir(parents=True)
    script = Path(__file__).with_name(args.script)
    assert script.is_file(), script
    command = [str(engine/('UnrealEditor.exe' if args.render else 'UnrealEditor-Cmd.exe')),
        str(ROOT/'UPlayground.uproject'), '-unattended', '-nosound', '-nosplash', '-nop4',
        '-culture=en', '-DisablePlugins=RiderLink', '-EnablePlugins=PythonScriptPlugin',
        '-ddc=InstalledNoZenLocalFallback', '-Multiprocess', '-abslog='+str(run/'editor.log')]
    if args.render:
        command += ['-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1600', '-ResY=900',
            '-ExecutePythonScript='+str(script), '-PGTestProfile=LevelDressing_'+run.name,
            '-UserDir='+str(run/'User')]
    else:
        command += ['-nullrhi', '-run=pythonscript', '-script='+str(script)]
    (run/'command.json').write_text(json.dumps(command, indent=2))
    print(str(run), flush=True)
    with (run/'stdout.log').open('w', encoding='utf-8') as log:
        result = subprocess.run(command, cwd=ROOT, env=dict(os.environ, PG_DRESSING_RUN=str(run)),
            stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW, timeout=600)
    print('Editor exit:', result.returncode, flush=True)
    preview=run/'preview.json'
    if args.render and preview.is_file():
        report=json.loads(preview.read_text(encoding='utf-8'))
        report['editor_exit_code']=result.returncode
        report['editor_shutdown_ok']=result.returncode==0
        payload=json.dumps(report,ensure_ascii=False,indent=2)
        preview.write_text(payload,encoding='utf-8')
        (ROOT/'Saved/LevelDressing/latest_preview.json').write_text(payload,encoding='utf-8')
    raise SystemExit(result.returncode)

if __name__ == '__main__':
    main()
