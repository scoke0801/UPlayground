"""Run authoring, reload validation or an isolated actual-gameplay character probe."""
import argparse
import json
import os
import uuid
from datetime import datetime, timezone
from pathlib import Path
from RunQA import ROOT, run_process, read_text, FATAL, check_automation

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step',choices=['inspect','configure','validate','runtime','automation'],required=True)
    parser.add_argument('--render',action='store_true')
    args=parser.parse_args()
    out=ROOT/'Saved/PlayableCharacters/Runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')+'_'+args.step)
    out.mkdir(parents=True)
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games/UE_5.8/Engine/Binaries/Win64'
    exe='UnrealEditor.exe' if args.render else 'UnrealEditor-Cmd.exe'
    cmd=[engine/exe,ROOT/'UPlayground.uproject']
    if args.step=='runtime':
        cmd+=['/Game/Maps/RogueArena','-game','-ExecCmds=t.MaxFPS 60,PGCharacterProbe','-PGTestProfile=Characters_'+uuid.uuid4().hex[:12]]
        if args.render: cmd+=['-PGCharacterCapture']
    elif args.step=='automation':
        cmd+=['-ExecCmds=Automation RunTests PG.','-TestExit=Automation Test Queue Empty','-ReportExportPath='+str(out/'Automation'),'-PGTestProfile=Characters_Automation_'+uuid.uuid4().hex[:8]]
    else:
        script={'inspect':'Inspect','configure':'Configure','validate':'Validate'}[args.step]+'PlayableCharacters.py'
        cmd+=['-EnablePlugins=PythonScriptPlugin','-run=pythonscript','-script='+str(ROOT/'Tools/Validation'/script)]
    cmd+=['-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720'] if args.render else ['-nullrhi']
    cmd+=['-unattended','-nosound','-nop4','-culture=en','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback','-abslog='+str(out/'engine.log')]
    (out/'command.json').write_text(json.dumps([str(x) for x in cmd],indent=2))
    code,timeout=run_process(cmd,ROOT,out/'stdout.log',900)
    log=read_text(out/'engine.log')
    ok=code==0 and not timeout and not FATAL.search(log)
    if args.step=='runtime': ok=ok and 'PGCharacterProbe PASS players=7 monsters=4' in log
    elif args.step=='automation':
        errors,count=check_automation(json.loads(read_text(out/'Automation/index.json')),['PG.Progression.TransactionsAndSerialization'])
        ok=ok and not errors
    elif args.step!='inspect': ok=ok and json.loads((ROOT/'Saved/PlayableCharacters'/(args.step+'.json')).read_text(encoding='utf-8'))['status']=='PASS'
    result=dict(status='PASS' if ok else 'FAIL',code=code,timeout=timeout,rendered=args.render,direct_input=False)
    (out/'result.json').write_text(json.dumps(result,indent=2))
    print(result,out,flush=True)
    return 0 if ok else 1

if __name__=='__main__': raise SystemExit(main())
