"""Back up, upgrade and reload toon masters in bounded, hidden UE processes."""
import argparse
from datetime import datetime, timezone
import json
import os
import re
from pathlib import Path
from RunQA import ROOT, run_process, read_text, FATAL
from PlayableCharacterTransaction import restore, write_json


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step',choices=['apply','validate','automation','render','native-render','faces-render','hair-render'],default='apply')
    parser.add_argument('--run',type=Path)
    parser.add_argument('--p09-only',action='store_true')
    parser.add_argument('--final-assets',action='store_true')
    parser.add_argument('--hair-softness',action='store_true')
    parser.add_argument('--improved',action='store_true')
    parser.add_argument('--all-tests',action='store_true')
    parser.add_argument('--controls',action='store_true')
    args=parser.parse_args()
    base=ROOT/'Saved/ToonImprovement'
    out=(args.run or base/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')).resolve()
    out.mkdir(parents=True,exist_ok=True)
    os.environ['PG_TOON_UPGRADE_RUN']=str(out)
    engine=Path('C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64')
    common=['-unattended','-nop4','-nosound','-culture=en','-DisablePlugins=RiderLink',
            '-ddc=InstalledNoZenLocalFallback','-EnablePlugins=PythonScriptPlugin',
            '-UserDir='+str(out/'User'),'-PGTestProfile=ToonUpgrade_'+out.name]
    if args.p09_only:common+=['-PGP09Only']
    if args.improved:common+=['-PGImprovedMap']
    if args.controls:common+=['-PGToonControls']
    gates=[]
    try:
        for step in (['apply','validate'] if args.step=='apply' else [args.step]):
            flags=['-nullrhi']
            render=step in ['render','native-render','faces-render','hair-render']
            if render:
                flags=['-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720',
                       '-ExecutePythonScript='+str(ROOT/'Tools/Validation'/('PreviewCharacterFaces.py' if step=='faces-render' else 'PreviewToonImprovement.py'))]
                if step=='native-render':flags+=['-PGNativeToon',
                    '-ini:Engine:[/Script/Engine.RendererSettings]:r.Substrate=1,[/Script/Engine.RendererSettings]:r.Substrate.ProjectGBufferFormat=0']
                if step=='hair-render':flags+=['-PGHairComparison']
            elif step=='automation':
                flags+=['-ExecCmds=Automation RunTests '+('PG.' if args.all_tests else 'PG.Rendering.Toon.'), '-TestExit=Automation Test Queue Empty',
                        '-ReportExportPath='+str(out/'Automation')]
            else:
                flags+=['-run=pythonscript','-script='+str(ROOT/'Tools/Validation'/('ConfigureHairSoftness.py' if args.hair_softness else 'ConfigureToonFinalAssets.py' if args.final_assets else 'ConfigureToonImprovement.py'))]
                if step=='validate': flags+=['-PGToonValidate']
            code,timeout=run_process([engine/('UnrealEditor.exe' if render else 'UnrealEditor-Cmd.exe'),ROOT/'UPlayground.uproject']+common+flags+
                ['-abslog='+str(out/(step+'.log'))],ROOT,out/(step+'.stdout.log'),600)
            log=read_text(out/(step+'.log'))
            assert not code and not timeout and not FATAL.search(log) and 'LogPython: Error' not in log and not re.search(r'Failed to compile Material|LogShaderCompilers: Error',log), (step,code,timeout)
            if step!='automation':
                result=json.loads(read_text(out/('render.json' if render else step+'.json')))
                assert result['status']=='PASS',result
            else:
                result=json.loads(read_text(out/'Automation/index.json'))
                assert result['failed']==0 and result['succeeded']+result.get('succeededWithWarnings',0)>0,result
            gates.append(dict(step=step,status='PASS'))
        if args.step=='apply':
            tx=json.loads(read_text(out/'transaction.json'));tx['status']='VERIFIED';write_json(out/'transaction.json',tx)
            (base/'latest.txt').write_text(str(out),encoding='utf-8')
        write_json(out/'run.json',dict(status='PASS',gates=gates))
    except BaseException as error:
        if args.step=='apply' and (out/'transaction.json').exists(): restore(ROOT,out)
        write_json(out/'run.json',dict(status='FAIL',gates=gates,error=str(error)))
        raise
    finally: print(out,flush=True)


if __name__=='__main__': main()
