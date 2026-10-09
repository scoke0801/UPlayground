"""Import HUD art and verify actual game rendering with isolated consumable probes."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from RunQA import run_process, read_text, FATAL

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--step',choices=['import','runtime','preview','all'],default='all')
    parser.add_argument('--width',type=int,default=1600)
    parser.add_argument('--height',type=int,default=900)
    args=parser.parse_args()
    version=json.loads((ROOT/'UPlayground.uproject').read_text(encoding='utf-8-sig'))['EngineAssociation']
    binary=Path(os.environ.get('ProgramFiles','C:/Program Files'))/f'Epic Games/UE_{version}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    run=ROOT/'Saved/CombatHUD'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    run.mkdir(parents=True)
    flags=['-unattended','-nosound','-nop4','-culture=ko','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback']
    cases=[]
    if args.step in ['all','import']:
        cases.append(('import',['-nullrhi','-EnablePlugins=PythonScriptPlugin','-run=pythonscript','-script='+str(Path(__file__).with_name('ImportCombatHUD.py'))],'PGCombatHUD IMPORT PASS'))
    if args.step in ['all','runtime']:
        cases.append(('runtime',['/Game/Maps/L_PG_ForestRuins','-game','-RenderOffscreen','-windowed','-ForceRes',f'-ResX={args.width}',f'-ResY={args.height}',
                                 '-UseFixedTimeStep','-FPS=60','-ExecCmds=t.MaxFPS 60,DisableAllScreenMessages,PGConsumableProbe'],'PGConsumableProbe PASS'))
    if args.step in ['all','preview']:
        cases.append(('presentation',['-RenderOffscreen','-windowed','-ForceRes',f'-ResX={args.width}',f'-ResY={args.height}',
                     '-EnablePlugins=PythonScriptPlugin','-ExecutePythonScript='+str(Path(__file__).with_name('PreviewCombatHUD.py'))],'PGCombatHUD PRESENTATION PASS'))
    results=[]
    print(run,flush=True)
    for name,extra,marker in cases:
        executable=binary.with_name('UnrealEditor.exe') if name=='presentation' else binary
        prefix='CombatHUD_' if name=='presentation' else 'Consumables_HUD_'
        cmd=[executable,ROOT/'UPlayground.uproject']+flags+extra+['-PGTestProfile='+prefix+run.name,
             '-UserDir='+str(run/name/'User'),'-abslog='+str(run/(name+'.log'))]
        os.environ['PG_COMBAT_HUD_OUTPUT']=str(run/'presentation')
        os.environ['PG_COMBAT_HUD_WIDTH']=str(args.width)
        os.environ['PG_COMBAT_HUD_HEIGHT']=str(args.height)
        code,timeout=run_process(cmd,ROOT,run/(name+'.stdout.log'),240)
        log=read_text(run/(name+'.log'))
        errors=[]
        if code or timeout or FATAL.search(log) or marker not in log or 'LogPython: Error' in log:errors.append(dict(code=code,timeout=timeout,marker=marker in log))
        images=[]
        if name=='runtime':
            for shot in ['ready','low_health','healed','empty']:
                path=run/name/'User/Saved/QA/Consumables'/(shot+'.png')
                if not path.is_file():errors.append('Missing '+shot)
                images.append(str(path))
        if name=='presentation':
            path=run/'presentation/report.json'
            if not path.exists():errors.append('Missing presentation report')
            else:
                presentation=json.loads(path.read_text(encoding='utf-8'))
                if presentation['status']!='PASS':errors.append(presentation.get('error','Presentation failed'))
                images=presentation.get('images',[])
                if len(images)!=3 or not all(Path(p).is_file() for p in images):errors.append('Missing presentation images')
        results.append(dict(name=name,errors=errors,images=images,command=[str(x) for x in cmd]))
        print(name+': '+('FAIL' if errors else 'PASS'),flush=True)
        if errors:break
    ok=not any(r['errors'] for r in results)
    (run/'report.json').write_text(json.dumps(dict(status='PASS' if ok else 'FAIL',resolution=[args.width,args.height],cases=results),indent=2),encoding='utf-8')
    print(run/'report.json',flush=True)
    return 0 if ok else 1

if __name__=='__main__':raise SystemExit(main())
