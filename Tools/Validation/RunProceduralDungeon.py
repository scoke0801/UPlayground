"""Reproduce P0 assets, structure tests, 100-seed PIE navigation and optional renders."""
import argparse,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
    p=argparse.ArgumentParser();p.add_argument('--build-map',action='store_true');p.add_argument('--render',action='store_true');p.add_argument('--runtime',action='store_true');args=p.parse_args()
    out=ROOT/'Saved/QA/ProceduralDungeon';out.mkdir(parents=True,exist_ok=True)
    version=json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation']
    engine=Path('C:/Program Files/Epic Games')/('UE_'+version)/'Engine/Binaries/Win64'
    cases=[]
    if args.build_map:cases.append(('map_build','UnrealEditor-Cmd.exe',['-nullrhi','-run=pythonscript','-script='+str(ROOT/'Tools/Art/ProceduralDungeon/BuildProceduralDungeon.py')],'PGDungeon MAP SAVED'))
    cases.append(('automation','UnrealEditor-Cmd.exe',['-nullrhi','-ExecCmds=Automation RunTests PG.Dungeon; Quit','-TestExit=Automation Test Queue Empty','-ReportExportPath='+str(out/'Automation')],'TEST COMPLETE. EXIT CODE: 0'))
    if args.runtime:cases.append(('runtime','UnrealEditor.exe',['-nullrhi','-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeProceduralDungeon.py')],'PGDungeon PROBE PASS'))
    if args.render:cases.append(('render','UnrealEditor.exe',['-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-PGDungeonRender','-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeProceduralDungeon.py')],'PGDungeon PROBE PASS'))
    results=[]
    for name,exe,extra,marker in cases:
        cmd=[str(engine/exe),str(ROOT/'UPlayground.uproject'),'-PGDungeonPreview','-unattended','-nosound','-nop4','-culture=en','-DisablePlugins=RiderLink','-EnablePlugins=PythonScriptPlugin','-ddc=InstalledNoZenLocalFallback','-PGTestProfile=DungeonProbe','-UserDir='+str(out/'User'),'-abslog='+str(out/(name+'.log'))]+extra
        with (out/(name+'.stdout.log')).open('w',encoding='utf-8') as log:
            proc=subprocess.Popen(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
            try:code=proc.wait(timeout=1200)
            except subprocess.TimeoutExpired:proc.kill();proc.wait();raise
        text=(out/(name+'.log')).read_text(encoding='utf-8',errors='replace')
        ok=code==0 and marker in text and 'LogPython: Error' not in text
        if name in ['runtime','render'] and (out/(name+'.json')).exists():
            report=json.loads((out/(name+'.json')).read_text(encoding='utf-8'))
            report['editor_exit_code']=code
            report['editor_shutdown_ok']=code==0
            if code!=0:report['status']='FAIL_SHUTDOWN' if report.get('status')=='PASS' else 'FAIL'
            (out/(name+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        results.append(dict(step=name,exit_code=code,status='PASS' if ok else 'FAIL'))
        (out/'result.json').write_text(json.dumps(results,indent=2))
        print(results[-1],flush=True)
        if not ok:raise RuntimeError(str(out/(name+'.log')))
if __name__=='__main__':main()
