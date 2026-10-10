"""Bounded editor/package forest combat workload with finalized CSV evidence."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
from RunQA import ROOT,run_process,read_text,FATAL
from RunToonPerformance import metrics,csv_complete,processes,gpu_sample

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packaged-exe',type=Path)
    parser.add_argument('--camera',choices=['quarter','action','both'],default='both')
    args=parser.parse_args()
    assert not processes(),'Another Unreal/game process is running; do not contaminate performance samples'
    out=ROOT/'Saved/ToonImprovement'/('Scene_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    out.mkdir(parents=True)
    print(out,flush=True)
    engine=Path('C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe')
    report=dict(status='RUNNING',packaged=bool(args.packaged_exe),gpu=gpu_sample(),cameras=[],
        limits=['60-second scripted workload per camera after warmup; not human-play or long-duration certification',
                'Real authored stage/AI/GAS; player health restored; results characterize this machine only'])
    for mode in (['quarter','action'] if args.camera=='both' else [args.camera]):
        dest=out/mode;dest.mkdir()
        command=([args.packaged_exe] if args.packaged_exe else [engine,ROOT/'UPlayground.uproject'])+[
            '/Game/Maps/L_PG_ForestRuins','-game','-RenderOffscreen','-windowed','-ForceRes','-ResX=1920','-ResY=1080',
            '-nosound','-nop4','-unattended','-culture=en','-csvGpuStats','-csvAllCategoriesDisabled','-csvCategories=Basic,Global,GPU',
            '-DisablePlugins=RiderLink','-ddc=InstalledNoZenLocalFallback',
            '-PGTestProfile=ToonScene_'+out.name+'_'+mode,'-UserDir='+str(dest/'User'),'-abslog='+str(dest/'scene.log'),
            '-ExecCmds=PGToonSceneProbe']
        if mode=='action':command+=['-PGToonActionCamera']
        code,timeout=run_process(command,ROOT,dest/'stdout.log',240)
        log=read_text(dest/'scene.log')
        assert not code and not timeout and not FATAL.search(log) and 'PGToonScene PASS' in log,(mode,code,timeout)
        csvs=list(dest.rglob('ToonScene.csv'))
        assert len(csvs)==1 and csv_complete(csvs[0]),csvs
        data=metrics(csvs[0])
        assert all(data.get(k,{}).get('samples',0)>500 for k in ['FrameTime','GPUTime','GameThreadTime','RenderThreadTime']),data
        row=dict(camera=mode,status='PASS',command=[str(c) for c in command],csv=str(csvs[0]),metrics=data,
            workload=re.findall(r'PGToonScene PASS[^\r\n]+',log))
        report['cameras'].append(row)
        (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    report['status']='PASS'
    (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({r['camera']:{k:r['metrics'][k] for k in ['FrameTime','GPUTime']} for r in report['cameras']},indent=2),flush=True)

if __name__=='__main__':main()
