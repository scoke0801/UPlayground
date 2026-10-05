"""Run the player prefab import, gallery render or fresh-process validation."""
import argparse
import json
import os
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/PlayerModels'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step',required=True,choices=['configure','preview','validate'])
    parser.add_argument('--reimport',choices=['Hwarin','Arin','Yura'],help='Explicitly replace this generated skeletal mesh from its FBX')
    args=parser.parse_args()
    if args.reimport and args.step!='configure':parser.error('--reimport requires --step configure')
    run=OUT/'Runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')+'_'+args.step)
    run.mkdir(parents=True)
    script=ROOT/'Tools/Validation'/({'configure':'Configure','preview':'Preview','validate':'Validate'}[args.step]+'ToonCharacterBatch.py')
    engine=Path('C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64')
    render=args.step=='preview'
    command=[str(engine/('UnrealEditor.exe' if render else 'UnrealEditor-Cmd.exe')),str(ROOT/'UPlayground.uproject'),
        '-unattended','-nosound','-nosplash','-nop4','-culture=en','-DisablePlugins=RiderLink',
        '-EnablePlugins=PythonScriptPlugin','-ddc=InstalledNoZenLocalFallback','-abslog='+str(run/'engine.log')]
    command+=['-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720','-ExecutePythonScript='+str(script)] if render else ['-nullrhi','-run=pythonscript','-script='+str(script)]
    env=dict(os.environ,PG_CHARACTER_BATCH_MANIFEST=str(Path(__file__).with_name('manifest.json')),PG_CHARACTER_BATCH_OUTPUT=str(OUT))
    if args.reimport:env['PG_CHARACTER_BATCH_REIMPORT']=args.reimport
    latest=OUT/(args.step+'.json');previous=latest.stat().st_mtime_ns if latest.exists() else None
    (run/'command.json').write_text(json.dumps(command,indent=2))
    with (run/'stdout.log').open('w',encoding='utf-8') as log:
        p=subprocess.Popen(command,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
        try:code=p.wait(timeout=1800)
        except BaseException:
            subprocess.run(['taskkill','/PID',str(p.pid),'/T','/F'],capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
            raise
    assert latest.exists() and latest.stat().st_mtime_ns!=previous,('No fresh report',run)
    report=json.loads(latest.read_text(encoding='utf-8'))
    assert code==0 and report['status'] in ('PASS','CAPTURED'),(code,report.get('error'),str(run))
    print(args.step,report['status'],run)


if __name__=='__main__':main()
