"""Optional backed-up apply, fresh reload, then six actual stage-spawn checks."""
import argparse
from datetime import datetime,timezone
import json
import os
from pathlib import Path
from RunQA import ROOT,run_process,read_text,FATAL


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--apply',action='store_true')
    parser.add_argument('--runtime-only',action='store_true')
    args=parser.parse_args()
    out=ROOT/'Saved/QA'/('P09Waves_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S'))
    out.mkdir(parents=True)
    version=json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/('UE_'+version)/'Engine/Binaries/Win64'
    flags=['-nullrhi','-unattended','-nop4','-nosound','-culture=en','-DisablePlugins=RiderLink',
        '-ddc=InstalledNoZenLocalFallback','-EnablePlugins=PythonScriptPlugin',
        '-UserDir='+str(out/'User'),'-PGTestProfile='+out.name]
    script=str(ROOT/'Tools/Validation/ConfigureP09Waves.py')
    cases=[]
    if args.apply:cases.append(('apply','UnrealEditor-Cmd.exe',['-run=pythonscript','-script='+script],'PGP09Waves APPLY PASS'))
    if not args.runtime_only:cases.append(('reload','UnrealEditor-Cmd.exe',['-run=pythonscript','-script='+script,'-PGP09WavesValidate'],'PGP09Waves VALIDATION PASS'))
    cases.append(('runtime','UnrealEditor.exe',['-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeP09WavesPIE.py'),'-PGP09Evidence='+str(out)],'PGP09WaveProbe PASS'))
    results=[]
    print(out,flush=True)
    for name,exe,extra,marker in cases:
        cmd=[engine/exe,ROOT/'UPlayground.uproject']+flags+extra+['-abslog='+str(out/(name+'.log'))]
        code,timeout=run_process(cmd,ROOT,out/(name+'.stdout.log'),300)
        log=read_text(out/(name+'.log')) if (out/(name+'.log')).exists() else ''
        fail=bool(code or timeout or FATAL.search(log) or marker not in log or 'LogPython: Error' in log)
        results.append(dict(gate=name,status='FAIL' if fail else 'PASS',code=code,timeout=timeout,command=[str(x) for x in cmd]))
        print(name+': '+results[-1]['status'],flush=True)
        if fail:break
    report=dict(status='FAIL' if any(r['status']=='FAIL' for r in results) else 'PASS',gates=results)
    (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return report['status']!='PASS'


if __name__=='__main__':raise SystemExit(main())
