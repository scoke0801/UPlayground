"""Backed-up apply, fresh reload and rendered two-skill GAS verification."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
from RunQA import ROOT, run_process, read_text, FATAL


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--apply',action='store_true')
    parser.add_argument('--render-only',action='store_true')
    args=parser.parse_args()
    out=ROOT/'Saved/QA'/('SkeletonArcher_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S'))
    out.mkdir(parents=True)
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games/UE_5.8/Engine/Binaries/Win64'
    flags=['-unattended','-nop4','-nosound','-culture=en','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback',
           '-EnablePlugins=PythonScriptPlugin','-UserDir='+str(out/'User'),'-PGTestProfile=Archer_'+out.name]
    cases=[]
    script=str(ROOT/'Tools/Validation/ConfigureSkeletonArcher.py')
    if args.apply: cases.append(('apply','UnrealEditor-Cmd.exe',['-nullrhi','-run=pythonscript','-script='+script],'PGSkeletonArcher APPLY PASS'))
    if not args.render_only:
        cases.append(('reload','UnrealEditor-Cmd.exe',['-nullrhi','-run=pythonscript','-script='+script,'-PGArcherValidate'],'PGSkeletonArcher VALIDATION PASS'))
    cases.append(('render','UnrealEditor.exe',['-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeSkeletonArcherPIE.py'),
        '-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-PGArcherEvidence='+str(out)],'PGArcherProbe PASS'))
    results=[]
    print(out,flush=True)
    for name,exe,extra,marker in cases:
        command=[engine/exe,ROOT/'UPlayground.uproject']+flags+extra+['-abslog='+str(out/(name+'.log'))]
        code,timeout=run_process(command,ROOT,out/(name+'.stdout.log'),240)
        log=read_text(out/(name+'.log')) if (out/(name+'.log')).exists() else ''
        errors=[]
        if code or timeout or FATAL.search(log) or marker not in log or 'LogPython: Error' in log:
            errors.append(dict(code=code,timeout=timeout,missing_marker=marker not in log))
        if name=='render' and not errors:
            for sid in (15102,15112):
                for phase in ('aim','flight','arrow_detail'):
                    if not (out/(str(sid)+'_'+phase+'.png')).exists(): errors.append('Missing capture')
        results.append(dict(gate=name,errors=errors,command=[str(x) for x in command]))
        print(name+(': FAIL '+str(errors) if errors else ': PASS'),flush=True)
        if errors: break
    report=dict(status='FAIL' if any(r['errors'] for r in results) else 'PASS',gates=results)
    (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return report['status']!='PASS'


if __name__=='__main__': raise SystemExit(main())
