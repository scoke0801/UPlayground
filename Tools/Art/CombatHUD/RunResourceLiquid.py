"""Build, generate the UI material and exercise the existing HUD runtime probes."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from RunQA import run_process, read_text, FATAL

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--skip-build',action='store_true')
    parser.add_argument('--material-only',action='store_true')
    parser.add_argument('--preview-only',action='store_true')
    args=parser.parse_args()
    version=json.loads((ROOT/'UPlayground.uproject').read_text(encoding='utf-8-sig'))['EngineAssociation']
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/f'Epic Games/UE_{version}/Engine'
    out=ROOT/'Saved/CombatHUD'/('Liquid_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S'))
    out.mkdir(parents=True)
    print(out,flush=True)
    cases=[]
    if not args.skip_build and not args.material_only:
        cases.append(('build',[engine/'Build/BatchFiles/Build.bat','UPlaygroundEditor','Win64','Development',ROOT/'UPlayground.uproject','-WaitMutex','-NoHotReloadFromIDE'],None))
    cases.append(('material',[engine/'Binaries/Win64/UnrealEditor-Cmd.exe',ROOT/'UPlayground.uproject',
        '-unattended','-nosound','-nop4','-nullrhi','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback',
        '-EnablePlugins=PythonScriptPlugin','-run=pythonscript','-script='+str(Path(__file__).with_name('CreateResourceLiquid.py')),
        '-abslog='+str(out/'material.log')],'PGResourceLiquid CREATE PASS'))
    cases.append(('shader',[engine/'Binaries/Win64/UnrealEditor-Cmd.exe',ROOT/'UPlayground.uproject',
        '-unattended','-nosound','-nop4','-AllowCommandletRendering','-RenderOffscreen','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback',
        '-EnablePlugins=PythonScriptPlugin','-run=pythonscript','-script='+str(Path(__file__).with_name('ValidateResourceLiquid.py')),
        '-abslog='+str(out/'shader.log')],'PGResourceLiquid VALIDATE PASS'))
    for name,cmd,marker in ([] if args.preview_only else cases):
        code,timeout=run_process(cmd,ROOT,out/(name+'.stdout.log'),600)
        log=read_text(out/(name+'.log')) if marker else read_text(out/(name+'.stdout.log'))
        ok=not code and not timeout and not FATAL.search(log) and (not marker or marker in log) and 'LogPython: Error' not in log
        ok=ok and 'Failed to compile Material' not in log and 'LogShaderCompilers: Error' not in log
        print(name+': '+('PASS' if ok else 'FAIL'),flush=True)
        if not ok:
            print(log[-10000:],flush=True)
            return 1
    if args.material_only:return 0
    code=0 if args.preview_only else subprocess.call([sys.executable,str(Path(__file__).with_name('RunCombatHUD.py')),'--step','runtime'],cwd=ROOT)
    if code:return code
    os.environ['PG_COMBAT_HUD_OUTPUT']=str(out/'preview')
    cmd=[engine/'Binaries/Win64/UnrealEditor.exe',ROOT/'UPlayground.uproject','-unattended','-nosound','-nop4',
         '-culture=en','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback','-RenderOffscreen',
         '-windowed','-ForceRes','-ResX=1600','-ResY=900',
         '-EnablePlugins=PythonScriptPlugin','-ExecutePythonScript='+str(Path(__file__).with_name('PreviewResourceLiquid.py')),
         '-PGTestProfile=CombatHUD_Liquid_'+out.name,'-UserDir='+str(out/'User'),'-abslog='+str(out/'preview.log')]
    code,timeout=run_process(cmd,ROOT,out/'preview.stdout.log',240)
    log=read_text(out/'preview.log')
    ok=not code and not timeout and 'PGResourceLiquid PREVIEW PASS' in log and not FATAL.search(log)
    ok=ok and 'Failed to compile Material' not in log and 'LogShaderCompilers: Error' not in log
    if ok:
        # Stable half-health reservoir must animate even when the numeric value is unchanged.
        measurement=subprocess.run(['powershell.exe','-NoProfile','-File',str(Path(__file__).with_name('MeasureResourceLiquid.ps1')),
                                    '-PreviewPath',str(out/'preview')],capture_output=True,text=True)
        ok=measurement.returncode==0
        (out/'animation.json').write_text(measurement.stdout,encoding='utf-8')
    print('preview: '+('PASS' if ok else 'FAIL'),flush=True)
    (out/'result.json').write_text(json.dumps(dict(status='PASS' if ok else 'FAIL',exit_code=code,timeout=timeout),indent=2))
    return 0 if ok else 1

if __name__=='__main__':raise SystemExit(main())
