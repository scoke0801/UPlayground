"""Reload/PG regressions and real GAS dash probes in disposable profiles."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, check_automation

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--apply',action='store_true')
    parser.add_argument('--render-only',action='store_true')
    parser.add_argument('--all-characters',action='store_true')
    parser.add_argument('--configuration',choices=('Development','DebugGame'),default='Development')
    args=parser.parse_args()
    run=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_')+uuid.uuid4().hex[:6]
    out=ROOT/'Saved/QA'/(run+'_player_dash'); out.mkdir(parents=True)
    executable='UnrealEditor-Win64-DebugGame.exe' if args.configuration=='DebugGame' else 'UnrealEditor-Cmd.exe'
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games/UE_5.8/Engine/Binaries/Win64'/executable
    common=[engine,ROOT/'UPlayground.uproject']
    flags=['-unattended','-nop4','-nosound','-culture=en','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback']
    cases=[]
    if not args.render_only:
        if args.apply:
            cases.append(('apply',['-nullrhi','-EnablePlugins=PythonScriptPlugin','-run=pythonscript','-script='+str(ROOT/'Tools/Validation/ConfigurePlayerDash.py')],'PGDash APPLY PASS'))
        cases.append(('reload',['-nullrhi','-EnablePlugins=PythonScriptPlugin','-run=pythonscript','-script='+str(ROOT/'Tools/Validation/ConfigurePlayerDash.py'),'-PGDashValidate'],'PGDash VALIDATION PASS'))
        cases.append(('automation',['-nullrhi','-ExecCmds=Automation RunTests PG.','-TestExit=Automation Test Queue Empty','-ReportExportPath='+str(out/'Automation')],None))
    for identity in (['Bokusei','LianLian','Honoka','Hichi','Siuha','Lili','Nenmir'] if args.all_characters else ['Bokusei']):
        cases.append((identity,['/Game/Maps/RogueArena','-game','-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720','-UseFixedTimeStep','-FPS=60',
            '-PGDashCharacter='+identity,'-PGDashCapture','-ExecCmds=t.MaxFPS 60,PGDashProbe'],'PGDashProbe PASS'))
    results=[]
    print(out,flush=True)
    for name,extra,marker in cases:
        cmd=common+extra+flags+['-PGTestProfile=Dash_'+run+'_'+name,'-UserDir='+str(out/name/'User'),'-abslog='+str(out/(name+'.log'))]
        code,timeout=run_process(cmd,ROOT,out/(name+'.stdout.log'),240)
        log=read_text(out/(name+'.log')) if (out/(name+'.log')).exists() else ''
        errors=[]
        if code or timeout or FATAL.search(log) or (marker and marker not in log): errors.append(f'code={code} timeout={timeout} missing_marker={bool(marker and marker not in log)}')
        if 'LogPython: Error' in log or 'Failed to compile Material' in log: errors.append('Python/material error')
        if name=='automation':
            report=out/'Automation/index.json'
            if report.exists(): errors.extend(check_automation(json.loads(read_text(report)),['PG.Combat.DodgeDirection'])[0])
            else: errors.append('Missing automation report')
        if '-PGDashCapture' in extra and not (out/name/'User/Saved/QA/PlayerDash/Dash.png').exists(): errors.append('Missing dash capture')
        results.append(dict(name=name,errors=errors,command=[str(x) for x in cmd]))
        print(name+(': FAIL '+str(errors) if errors else ': PASS'),flush=True)
        if errors: break
    result=dict(status='FAIL' if any(x['errors'] for x in results) else 'PASS',gates=results,direct_keyboard_play_verified=False)
    (out/'report.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(out/'report.json',flush=True)
    return result['status']!='PASS'

if __name__=='__main__': raise SystemExit(main())
