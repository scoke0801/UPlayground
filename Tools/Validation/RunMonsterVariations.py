"""Apply with rollback, reload in a fresh UE process, then optional runtime probe."""
import argparse
from datetime import datetime,timezone
import json
import os
from pathlib import Path
from RunQA import ROOT,run_process,read_text,FATAL
from PlayableCharacterTransaction import restore,write_json

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--apply',action='store_true')
    parser.add_argument('--grips-only',action='store_true',help='Apply/reload only the six P09 attachment profiles')
    parser.add_argument('--runtime',action='store_true')
    parser.add_argument('--render',action='store_true')
    parser.add_argument('--enemy',type=int,help='Isolate one enemy in the runtime cast probe')
    args=parser.parse_args()
    out=ROOT/'Saved/QA'/('MonsterVariations_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f'))
    out.mkdir(parents=True)
    os.environ['PG_MONSTER_RUN']=str(out)
    version=json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/('UE_'+version)/'Engine/Binaries/Win64'
    flags=['-unattended','-nop4','-nosound','-culture=en','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback','-EnablePlugins=PythonScriptPlugin','-UserDir='+str(out/'User'),'-PGTestProfile='+out.name]
    if args.enemy:flags.append('-PGMonsterOnly='+str(args.enemy))
    script=str(ROOT/'Tools/Validation/ConfigureMonsterVariations.py')
    cases=[]
    marker='PGMonsterGrips' if args.grips_only else 'PGMonsterVariations'
    if args.apply: cases.append(('apply','UnrealEditor-Cmd.exe',['-nullrhi','-run=pythonscript','-script='+script]+(['-PGMonsterGripsOnly'] if args.grips_only else []),marker+' APPLY PASS'))
    cases.append(('reload','UnrealEditor-Cmd.exe',['-nullrhi','-run=pythonscript','-script='+script,'-PGMonsterGripsValidate' if args.grips_only else '-PGMonsterValidate'],marker+' VALIDATION PASS'))
    if args.runtime or args.render:
        cases.append(('runtime','UnrealEditor.exe',['-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeMonsterVariationsPIE.py'),'-PGMonsterEvidence='+str(out)]+(['-RenderOffscreen','-windowed','-ResX=1280','-ResY=720'] if args.render else ['-nullrhi']),'PGMonsterProbe PASS'))
    results=[]
    print(out,flush=True)
    for name,exe,extra,marker in cases:
        code,timeout=run_process([engine/exe,ROOT/'UPlayground.uproject']+flags+extra+['-abslog='+str(out/(name+'.log'))],ROOT,out/(name+'.stdout.log'),360)
        log=read_text(out/(name+'.log')) if (out/(name+'.log')).exists() else ''
        fail=bool(code or timeout or FATAL.search(log) or marker not in log or 'LogPython: Error' in log)
        results.append(dict(gate=name,status='FAIL' if fail else 'PASS',code=code,timeout=timeout))
        print(name+': '+results[-1]['status'],flush=True)
        if fail:
            if args.apply and (out/'transaction.json').exists(): restore(ROOT,out)
            break
    success=all(r['status']=='PASS' for r in results)
    if success and args.apply:
        transaction=json.loads((out/'transaction.json').read_text(encoding='utf-8'))
        transaction['status']='VERIFIED';write_json(out/'transaction.json',transaction)
    write_json(out/'report.json',dict(status='PASS' if success else 'FAIL',gates=results))
    return not success

if __name__=='__main__': raise SystemExit(main())
