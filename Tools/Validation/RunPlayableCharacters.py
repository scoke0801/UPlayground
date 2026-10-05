"""Run authoring, reload validation or an isolated actual-gameplay character probe."""
import argparse
import json
import os
import uuid
from datetime import datetime, timezone
from pathlib import Path
import subprocess
import re
import csv
import math

ROOT=Path(__file__).resolve().parents[2]
FATAL=re.compile(r'Fatal error:|Assertion failed:|Unhandled Exception:|Ensure condition failed:',re.I)

def read_text(path):
    return Path(path).read_text(encoding='utf-8-sig',errors='replace')

def run_process(command,cwd,output,timeout):
    with output.open('w',encoding='utf-8') as stream:
        process=subprocess.Popen([str(x) for x in command],cwd=cwd,stdout=stream,stderr=subprocess.STDOUT,
            stdin=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        try:
            return process.wait(timeout=timeout),False
        except BaseException:
            if os.name=='nt': subprocess.run(['taskkill','/PID',str(process.pid),'/T','/F'],capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=30)
            else: process.kill()
            process.wait(timeout=30)
            raise

def check_automation(report,expected):
    tests={t['fullTestPath']:t for t in report.get('tests',[])}
    errors=[name for name in expected if name not in tests]
    errors += [name for name,t in tests.items() if t.get('state')!='Success' or t.get('errors',0)]
    errors += [key for key in ('failed','notRun','inProcess') if report.get(key,-1)!=0]
    if not tests: errors.append('No tests ran')
    return errors,len(tests)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step',choices=['inspect','configure','validate','runtime','automation','polish-export','polish-validate','polish-check','grip-check'],required=True)
    parser.add_argument('--render',action='store_true')
    parser.add_argument('--runtime-project',type=Path,help='Frozen UPlayground.uproject for runtime/automation only')
    args=parser.parse_args()
    if args.runtime_project and args.step not in ('runtime','automation'):
        parser.error('--runtime-project is only supported for runtime/automation')
    if args.step == 'grip-check':
        if args.render: parser.error('grip-check measures combat invariance with NullRHI; it does not render visual acceptance')
        from RunPlayableCharacterGrip import main as grip_main
        return grip_main([])
    if args.step == 'configure' or args.step.startswith('polish-'):
        from RunPlayableCharacterPolish import main as polish_main
        return polish_main(['--step', args.step])
    out=ROOT/'Saved/PlayableCharacters/Runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')+'_'+args.step)
    out.mkdir(parents=True)
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games/UE_5.8/Engine/Binaries/Win64'
    exe='UnrealEditor.exe' if args.render else 'UnrealEditor-Cmd.exe'
    cmd=[engine/exe,args.runtime_project.resolve(strict=True) if args.runtime_project else ROOT/'UPlayground.uproject']
    if args.step=='runtime':
        cmd+=['/Game/Maps/RogueArena','-game','-ExecCmds=t.MaxFPS 60,PGCharacterProbe','-PGTestProfile=Characters_'+uuid.uuid4().hex[:12],
              '-PGCharacterProbeOutput='+str(out/'pose-samples.csv')]
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
    if args.step=='runtime':
        ok=ok and 'PGCharacterProbe PASS players=7 monsters=4' in log
        identities=['Bokusei','LianLian','Honoka','Hichi','Siuha','Lili','Nenmir']
        ok=ok and all('PGCharacterGrip Id='+identity+' TagIsolation=1 Refresh=1 InvalidFallback=1 DuplicateFallback=1' in log for identity in identities)
        with (out/'pose-samples.csv').open(encoding='utf-8-sig') as stream: samples=list(csv.DictReader(stream))
        for identity in identities:
            rows=[row for row in samples if row['identity']==identity]
            ok=ok and len({row['seconds'] for row in rows})>=10 and len({row['bone'] for row in rows})==4
        ok=ok and all(math.isfinite(float(value)) for row in samples for key,value in row.items() if key not in ('identity','bone'))
    elif args.step=='automation':
        errors,count=check_automation(json.loads(read_text(out/'Automation/index.json')),['PG.Progression.TransactionsAndSerialization'])
        ok=ok and not errors
    elif args.step!='inspect': ok=ok and json.loads((ROOT/'Saved/PlayableCharacters'/(args.step+'.json')).read_text(encoding='utf-8'))['status']=='PASS'
    result=dict(status='PASS' if ok else 'FAIL',code=code,timeout=timeout,rendered=args.render,direct_input=False)
    (out/'result.json').write_text(json.dumps(result,indent=2))
    print(result,out,flush=True)
    return 0 if ok else 1

if __name__=='__main__': raise SystemExit(main())
