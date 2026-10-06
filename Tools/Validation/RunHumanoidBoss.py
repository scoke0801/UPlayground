"""Back up/apply, validate saved reload, and probe both phases in isolated profiles."""
import argparse
from datetime import datetime,timezone
import json
import os
import struct
from pathlib import Path
from RunQA import ROOT,run_process,read_text,FATAL
from PlayableCharacterTransaction import restore,write_json

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--apply',action='store_true');parser.add_argument('--runtime',action='store_true');parser.add_argument('--render',action='store_true');parser.add_argument('--automation',action='store_true')
    parser.add_argument('--motions',action='store_true')
    args=parser.parse_args()
    out=ROOT/'Saved/QA'/('HumanoidBoss_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f'))
    out.mkdir(parents=True);os.environ['PG_HUMANOID_RUN']=str(out)
    version=json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/('UE_'+version)/'Engine/Binaries/Win64'
    flags=['-unattended','-nop4','-nosound','-culture=ko','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback','-EnablePlugins=PythonScriptPlugin','-UserDir='+str(out/'User'),'-PGTestProfile='+out.name]
    script=str(ROOT/'Tools/Validation/ConfigureHumanoidBoss.py')
    cases=[]
    if args.apply:cases.append(('apply','UnrealEditor-Cmd.exe',['-nullrhi','-run=pythonscript','-script='+script],'PGHumanoidBoss APPLY PASS'))
    cases.append(('reload','UnrealEditor-Cmd.exe',['-nullrhi','-run=pythonscript','-script='+script,'-PGHumanoidValidate'],'PGHumanoidBoss VALIDATION PASS'))
    # English avoids the engine's Korean numeric-format smoke-test failures.
    # The rendered gameplay probes retain Korean UI.
    if args.automation:cases.append(('automation','UnrealEditor-Cmd.exe',['-culture=en','-nullrhi','-ExecCmds=Automation RunTests PG.','-TestExit=Automation Test Queue Empty','-ReportExportPath='+str(out/'Automation')],''))
    if args.runtime or args.render:cases.append(('runtime','UnrealEditor.exe',['-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeHumanoidBossPIE.py'),'-PGHumanoidEvidence='+str(out)]+(['-RenderOffscreen','-windowed','-ResX=1280','-ResY=720'] if args.render else ['-nullrhi']),'PGHumanoidProbe PASS'))
    if args.motions:cases.append(('motions','UnrealEditor.exe',['-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeHumanoidMotions.py'),'-PGHumanoidEvidence='+str(out),'-RenderOffscreen','-windowed','-ResX=1280','-ResY=720'],'PGHumanoidMotion PASS'))
    results=[];print(out,flush=True)
    write_json(out/'report.json',dict(status='RUNNING',gates=results))
    for name,exe,extra,marker in cases:
        results.append(dict(gate=name,status='RUNNING'))
        write_json(out/'report.json',dict(status='RUNNING',gates=results))
        code,timeout=run_process([engine/exe,ROOT/'UPlayground.uproject']+flags+extra+['-abslog='+str(out/(name+'.log'))],ROOT,out/(name+'.stdout.log'),480)
        log=read_text(out/(name+'.log')) if (out/(name+'.log')).exists() else ''
        fail=bool(code or timeout or FATAL.search(log) or 'LogPython: Error' in log)
        if name=='automation':
            from RunQA import check_automation
            report=out/'Automation/index.json'
            fail=fail or not report.exists()
            if report.exists():fail=fail or bool(check_automation(json.loads(read_text(report)),['PG.Content.HumanoidBossData','PG.Content.HumanoidBossLifecycle'])[0])
        else:fail=fail or marker not in log
        if name in ('runtime','motions') and not fail:
            evidence=json.loads(read_text(out/('runtime.json' if name=='runtime' else 'motion-review.json')))
            fail=evidence['status']!='PASS'
            if name=='runtime':fail=fail or len(evidence['cases'])!=8
            if name=='motions':fail=fail or len(evidence['frames'])!=35
            if (args.render and name=='runtime') or name=='motions':
                captures=[out/(s['label']+'.png') for s in evidence['samples']] if name=='runtime' else [out/s['path'] for s in evidence['frames']]
                invalid=[]
                for p in captures:
                    png=p.read_bytes() if p.exists() else b''
                    size=struct.unpack('>II',png[16:24]) if len(png)>=24 and png[:8]==b'\x89PNG\r\n\x1a\n' else None
                    if size!=(1280,720):invalid.append(dict(path=p.name,size=size))
                if invalid:results[-1]['invalid_captures']=invalid
                fail=fail or bool(invalid)
        results[-1].update(status='FAIL' if fail else 'PASS',code=code,timeout=timeout)
        write_json(out/'report.json',dict(status='FAIL' if fail else 'RUNNING',gates=results))
        print(name+': '+results[-1]['status'],flush=True)
        if fail:
            if args.apply and (out/'transaction.json').exists():restore(ROOT,out)
            break
    success=all(r['status']=='PASS' for r in results)
    if success and args.apply:
        transaction=json.loads((out/'transaction.json').read_text(encoding='utf-8'));transaction['status']='VERIFIED';write_json(out/'transaction.json',transaction)
    write_json(out/'report.json',dict(status='PASS' if success else 'FAIL',gates=results))
    return not success

if __name__=='__main__':raise SystemExit(main())
