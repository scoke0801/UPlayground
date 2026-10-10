"""Apply, reload and optionally probe imported combat creatures in live PIE."""
import argparse,json,os,subprocess
from datetime import datetime,timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--apply',action='store_true');parser.add_argument('--runtime',action='store_true');parser.add_argument('--render',action='store_true');parser.add_argument('--run')
    args=parser.parse_args()
    out=Path(args.run) if args.run else ROOT/'Saved/QA'/('CreatureCombat_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S'))
    out=out.resolve()
    out.mkdir(parents=True,exist_ok=True);os.environ['PG_CREATURE_RUN']=str(out)
    engine=Path('C:/Program Files/Epic Games')/('UE_'+json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation'])/'Engine/Binaries/Win64'
    script=ROOT/'Tools/Validation/ConfigureCreatureCombat.py'
    cases=[]
    if args.apply:cases.append(('apply','UnrealEditor-Cmd.exe',['-nullrhi','-run=pythonscript','-script='+str(script)],'PGCreatureCombat VALIDATION PASS'))
    cases.append(('reload','UnrealEditor-Cmd.exe',['-nullrhi','-run=pythonscript','-script='+str(script),'-PGCreatureValidate'],'PGCreatureCombat VALIDATION PASS'))
    if args.runtime or args.render:cases.append(('runtime','UnrealEditor.exe',(['-RenderOffscreen','-windowed','-ResX=1280','-ResY=720'] if args.render else ['-nullrhi'])+['-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeCreatureCombat.py')],'PGCreatureProbe PASS'))
    print(out,flush=True);results=[]
    for name,exe,extra,marker in cases:
        command=[str(engine/exe),str(ROOT/'UPlayground.uproject'),'-unattended','-nosound','-nop4','-culture=en','-DisablePlugins=RiderLink','-EnablePlugins=PythonScriptPlugin','-ddc=InstalledNoZenLocalFallback','-UserDir='+str(out/'User'),'-PGTestProfile='+out.name,'-abslog='+str(out/(name+'.log'))]+extra
        with (out/(name+'.stdout.log')).open('w',encoding='utf-8') as log:
            p=subprocess.Popen(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
            try:code=p.wait(timeout=600)
            except subprocess.TimeoutExpired:p.kill();p.wait();raise
        log=(out/(name+'.log')).read_text(encoding='utf-8',errors='replace')
        passed=code==0 and marker in log and not any(s in log for s in ['LogPython: Error','Fatal error:'])
        results.append(dict(step=name,status='PASS' if passed else 'FAIL',code=code));print(results[-1],flush=True)
        (out/'result.json').write_text(json.dumps(results,indent=2))
        if not passed:raise RuntimeError(str(out/(name+'.log')))
    if (out/'transaction.json').exists():
        tx=json.loads((out/'transaction.json').read_text());tx['status']='VERIFIED';(out/'transaction.json').write_text(json.dumps(tx,indent=2))

if __name__=='__main__':main()
