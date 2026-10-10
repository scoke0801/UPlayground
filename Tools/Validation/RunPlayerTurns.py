"""Apply/reload/preview player turn assets with backups and isolated test profiles."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import time
from RunQA import ROOT, run_process, read_text, FATAL
from PlayableCharacterTransaction import restore, write_json

def wait_for_asset_writers():
    """Do not start a package transaction while another editor holds our target files."""
    if os.name!='nt':return
    import ctypes
    from ctypes import wintypes
    create=ctypes.windll.kernel32.CreateFileW
    create.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
    create.restype=wintypes.HANDLE
    close=ctypes.windll.kernel32.CloseHandle
    close.argtypes=[wintypes.HANDLE]
    files=[ROOT/'Content/Blueprints/Actor/LocalPlayer/Animation/ABP_LocalPlayer.uasset']
    files+=list((ROOT/'Content/DataCenter/PlayerTurns').glob('*.uasset'))
    deadline=time.monotonic()+180
    while True:
        locked=[]
        for file in files:
            handle=create(str(file),0x80000000,0,None,3,0,None)
            if handle==ctypes.c_void_p(-1).value:locked.append(str(file))
            else:close(handle)
        if not locked:return
        if time.monotonic()>deadline:raise RuntimeError('Target assets still in use: '+str(locked))
        print('Waiting for target asset handles to close: '+locked[0],flush=True)
        time.sleep(5)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--step',choices=['apply','validate','preview'],default='validate')
    parser.add_argument('--character',default='Bokusei')
    args=parser.parse_args()
    if args.step=='apply':wait_for_asset_writers()
    base=ROOT/'Saved/PlayerTurns';base.mkdir(parents=True,exist_ok=True)
    out=base/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    out.mkdir();os.environ['PG_TURNS_RUN']=str(out)
    version=json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/('UE_'+version)/'Engine/Binaries/Win64'
    common=['-unattended','-nop4','-nosound','-culture=en','-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback',
            '-EnablePlugins=PythonScriptPlugin','-UserDir='+str(out/'User'),'-PGTestProfile=PlayerTurns_'+out.name]
    steps=['apply','validate'] if args.step=='apply' else [args.step]
    gates=[]
    for step in steps:
        if step=='preview':
            executable='UnrealEditor.exe'
            flags=['/Game/Maps/RogueArena','-game','-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720',
                   '-UseFixedTimeStep','-FPS=60','-PGTurnCapture','-PGTurnCharacter='+args.character,'-ExecCmds=t.MaxFPS 60,PGPlayerTurnProbe']
        else:
            executable='UnrealEditor-Cmd.exe'
            flags=['-nullrhi','-run=pythonscript','-script='+str(ROOT/'Tools/Validation/ConfigurePlayerTurns.py')]
            if step=='validate':flags+=['-PGPlayerTurnsValidate']
        code,timeout=run_process([engine/executable,ROOT/'UPlayground.uproject']+common+flags+['-abslog='+str(out/(step+'.log'))],ROOT,out/(step+'.stdout.log'),300)
        log=read_text(out/(step+'.log')) if (out/(step+'.log')).exists() else ''
        marker='PGPlayerTurns '+('VALIDATION' if step=='validate' else step.upper())+' PASS'
        failed=bool(code or timeout or FATAL.search(log) or 'LogPython: Error' in log or 'LogBlueprint: Error' in log or marker not in log)
        if step=='preview' and not failed:
            import re
            cases=re.findall(r'PGTurn Case=(\d+) ',log)
            images=list((out/'User/Saved/QA/PlayerTurns').glob('Case*.png'))
            failed=cases!=[str(i) for i in range(28)] or len(images)!=224
        gates.append(dict(step=step,status='FAIL' if failed else 'PASS',code=code,timeout=timeout))
        print(json.dumps(gates[-1]),flush=True)
        if failed:
            if args.step=='apply' and (out/'transaction.json').exists():
                wait_for_asset_writers()
                restore(ROOT,out)
            break
    if not failed and args.step=='apply':
        tx=json.loads(read_text(out/'transaction.json'));tx['status']='VERIFIED';write_json(out/'transaction.json',tx)
        (base/'latest.txt').write_text(str(out),encoding='utf-8')
    write_json(out/'report.json',dict(status='FAIL' if failed else 'PASS',gates=gates))
    print(out,flush=True)
    return failed

if __name__=='__main__':raise SystemExit(main())
