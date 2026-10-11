"""Saved data inspection or transactional tempo update followed by fresh-process reload."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
from RunQA import ROOT, run_process, read_text, FATAL
from PlayableCharacterTransaction import restore, write_json

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--step',choices=['apply','inspect'],default='apply')
    args=parser.parse_args()
    out=ROOT/'Saved/QA'/('BossMotionTempo_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f'))
    out.mkdir(parents=True)
    os.environ['PG_BOSS_TEMPO_RUN']=str(out)
    version=json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    exe=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/('UE_'+version)/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    results=[]
    print(out,flush=True)
    for step in (['apply','validate'] if args.step=='apply' else [args.step]):
        os.environ['PG_BOSS_TEMPO_STEP']=step
        script='InspectBossMotionTempo.py' if step=='inspect' else 'ConfigureBossMotionTempo.py'
        code,timeout=run_process([exe,ROOT/'UPlayground.uproject','-unattended','-nop4','-nosound','-nullrhi','-culture=en',
            '-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback','-EnablePlugins=PythonScriptPlugin',
            '-UserDir='+str(out/'User'),'-PGTestProfile='+out.name,'-run=pythonscript',
            '-script='+str(ROOT/'Tools/Validation'/script),'-abslog='+str(out/(step+'.log'))],ROOT,out/(step+'.stdout.log'),300)
        log=read_text(out/(step+'.log')) if (out/(step+'.log')).exists() else ''
        ok=not(code or timeout or FATAL.search(log) or 'LogPython: Error' in log) and 'PGBossTempo '+step.upper()+' PASS' in log
        results.append(dict(step=step,status='PASS' if ok else 'FAIL',code=code,timeout=timeout))
        print(step+': '+results[-1]['status'],flush=True)
        if not ok:
            if args.step=='apply' and (out/'transaction.json').exists():restore(ROOT,out)
            break
    success=all(r['status']=='PASS' for r in results)
    if success and args.step=='apply':
        transaction=json.loads((out/'transaction.json').read_text(encoding='utf-8'))
        transaction['status']='VERIFIED';write_json(out/'transaction.json',transaction)
    write_json(out/'report.json',dict(status='PASS' if success else 'FAIL',gates=results))
    return not success

if __name__=='__main__':raise SystemExit(main())
