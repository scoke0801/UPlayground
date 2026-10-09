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
    args=parser.parse_args()
    version=json.loads((ROOT/'UPlayground.uproject').read_text(encoding='utf-8-sig'))['EngineAssociation']
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/f'Epic Games/UE_{version}/Engine'
    out=ROOT/'Saved/CombatHUD'/('Liquid_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S'))
    out.mkdir(parents=True)
    print(out,flush=True)
    cases=[]
    if not args.skip_build:
        cases.append(('build',[engine/'Build/BatchFiles/Build.bat','UPlaygroundEditor','Win64','Development',ROOT/'UPlayground.uproject','-WaitMutex','-NoHotReloadFromIDE'],None))
    cases.append(('material',[engine/'Binaries/Win64/UnrealEditor-Cmd.exe',ROOT/'UPlayground.uproject',
        '-unattended','-nosound','-nop4','-nullrhi','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback',
        '-EnablePlugins=PythonScriptPlugin','-run=pythonscript','-script='+str(Path(__file__).with_name('CreateResourceLiquid.py')),
        '-abslog='+str(out/'material.log')],'PGResourceLiquid CREATE PASS'))
    for name,cmd,marker in cases:
        code,timeout=run_process(cmd,ROOT,out/(name+'.stdout.log'),600)
        log=read_text(out/(name+'.log')) if marker else read_text(out/(name+'.stdout.log'))
        ok=not code and not timeout and not FATAL.search(log) and (not marker or marker in log) and 'LogPython: Error' not in log
        print(name+': '+('PASS' if ok else 'FAIL'),flush=True)
        if not ok:
            print(log[-10000:],flush=True)
            return 1
    return subprocess.call([sys.executable,str(Path(__file__).with_name('RunCombatHUD.py')),'--step','runtime'],cwd=ROOT)

if __name__=='__main__':raise SystemExit(main())
