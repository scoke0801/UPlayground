"""P1 saved assets, all native regression tests, and real GAS spatial probes.
This automated gate does not claim manual play or continuous motion acceptance.
"""
from datetime import datetime, timezone
import argparse
import json
import os
from pathlib import Path
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, check_automation
from HackSlashMetrics import check_spatial_observations

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--configuration',choices=('Development','DebugGame'),default='Development')
    args=parser.parse_args()
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/('UE_'+json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation'])
    run_id=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_')+uuid.uuid4().hex[:6]
    out=ROOT/'Saved/QA'/(run_id+'_hack_slash_p1'); out.mkdir(parents=True)
    executable='UnrealEditor-Win64-DebugGame-Cmd.exe' if args.configuration=='DebugGame' else 'UnrealEditor-Cmd.exe'
    common=[engine/'Engine/Binaries/Win64'/executable,ROOT/'UPlayground.uproject']
    flags=['-nullrhi','-unattended','-nop4','-nosound','-culture=en','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback','-PGTestProfile=HackSlash_P1_'+run_id]
    cases=[('assets',['-EnablePlugins=PythonScriptPlugin','-run=pythonscript','-script='+str(ROOT/'Tools/Validation/ValidateRoguelikeMVP.py')],'PGHackSlashP1 VALIDATION PASS'),
           ('automation',['-ExecCmds=Automation RunTests PG.','-TestExit=Automation Test Queue Empty','-ReportExportPath='+str(out/'Automation')],None),
           ('p0_spatial',['/Game/Maps/RogueArena','-game','-ExecCmds=t.MaxFPS 60,pg.Skill.Observe 1,PGHackSlashProbe'],'PGHackSlashProbe PASS skills=5'),
           ('p1_spatial',['/Game/Maps/RogueArena','-game','-PGHackSlashP1Probe','-ExecCmds=t.MaxFPS 60,pg.Skill.Observe 1,PGHackSlashProbe'],'PGHackSlashProbe PASS skills=3 p1=1'),
           ('default_slots',['/Game/Maps/RogueArena','-game','-PGDefaultSkillsProbe','-PGTestProfile=HackSlash_Default_'+run_id,'-ExecCmds=t.MaxFPS 60,pg.Skill.Observe 1,PGHackSlashProbe'],'PGHackSlashProbe PASS skills=4 default_slots=1')]
    gates=[]
    for name,extra,marker in cases:
        case_flags=[f for f in flags if not (name=='default_slots' and f.startswith('-PGTestProfile='))]
        command=common+extra+case_flags+['-abslog='+str(out/(name+'.log'))]
        code,timeout=run_process(command,ROOT,out/(name+'.stdout.log'),600)
        log=read_text(out/(name+'.log')) if (out/(name+'.log')).exists() else ''
        errors=[]
        if code or timeout or FATAL.search(log) or (marker and marker not in log): errors.append(f'code={code} timeout={timeout} missing_marker={bool(marker and marker not in log)}')
        if name=='automation':
            report=out/'Automation/index.json'
            if report.exists():
                more,_=check_automation(json.loads(read_text(report)),['PG.HackSlash.LoadoutTransactions','PG.HackSlash.CooldownIdentity','PG.HackSlash.ProjectileLifetimeAndSweep','PG.HackSlash.SpatialClockAndCancellation','PG.HackSlash.PoseContinuity','PG.Animation.HitStopDelta'])
                errors.extend(more)
            else: errors.append('Missing native test report')
        if name.endswith('spatial'):
            result=check_spatial_observations(log,p1=name=='p1_spatial'); errors.extend(result['errors'])
            (out/(name+'_observations.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
        gates.append(dict(name=name,status='FAIL' if errors else 'PASS',errors=errors,command=[str(c) for c in command]))
        print(name+': '+gates[-1]['status'],flush=True)
        if errors: break
    result=dict(status='FAIL' if any(g['errors'] for g in gates) else 'PASS',gates=gates,configuration=args.configuration,programmatic_input=True,
                direct_input_verified=False,p0_acceptance_complete=False,p1_acceptance_complete=False,p2_ready=False)
    (out/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(out/'report.json',flush=True)
    return int(result['status']!='PASS')
if __name__=='__main__': raise SystemExit(main())
