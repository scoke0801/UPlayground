"""Import, build and verify the forest ruins with isolated hidden UE processes."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step',choices=['prepare','normalize','import','detailed_import','ground','build','validate','preview','all'],default='all')
    parser.add_argument('--art-only',action='store_true',help='Preview cameras only; no combat validation claims')
    args=parser.parse_args()
    version=json.loads((ROOT/'UPlayground.uproject').read_text(encoding='utf-8'))['EngineAssociation']
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/('Epic Games/UE_'+version)/'Engine/Binaries/Win64'
    run=ROOT/'Saved/ForestRuins/Runs'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    run.mkdir(parents=True)
    steps=['prepare','normalize','import','ground','detailed_import','build','validate','preview'] if args.step=='all' else [args.step]
    scripts={'import':'InspectForestResources.py','detailed_import':'ImportDetailedResources.py','build':'BuildForestRuins.py','validate':'ValidateForestRuins.py','preview':'PreviewForestRuins.py'}
    for step in steps:
        if step=='prepare':
            subprocess.run([sys.executable,str(Path(__file__).with_name('PrepareDetailedResources.py'))],cwd=ROOT,check=True,timeout=180,
                           creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
            continue
        if step in ['ground','normalize']:
            candidates=list((Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Blender Foundation').glob('*/blender.exe'))
            assert candidates,'Blender executable not found'
            command=[str(sorted(candidates)[-1]),'--background','--factory-startup','--python',str(Path(__file__).with_name('BuildForestGround.py' if step=='ground' else 'NormalizeDetailedResources.py'))]
            with (run/(step+'_stdout.log')).open('w',encoding='utf-8') as log:
                subprocess.run(command,cwd=ROOT,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=90,
                               creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
            assert (Path(__file__).with_name('SM_PGFR_Ground.fbx' if step=='ground' else 'detailed_geometry.json')).is_file()
            print(step+': PASS '+str(run),flush=True)
            continue
        render=step=='preview'
        script=Path(__file__).with_name(scripts[step])
        assert script.is_file(),script
        command=[str(engine/('UnrealEditor.exe' if render else 'UnrealEditor-Cmd.exe')),str(ROOT/'UPlayground.uproject'),
                 '-unattended','-nosound','-nosplash','-nop4','-culture=en','-DisablePlugins=RiderLink',
                 '-EnablePlugins=PythonScriptPlugin','-ddc=InstalledNoZenLocalFallback','-Multiprocess',
                 '-abslog='+str(run/(step+'.log'))]
        if render:
            command+=['-RenderOffscreen','-windowed','-ForceRes','-ResX=1600','-ResY=900',
                      '-ExecutePythonScript='+str(script),'-PGTestProfile=ForestRuins_'+run.name,
                      '-UserDir='+str(run/'User'),'-ShaderWorkingDir='+str(ROOT/'Intermediate/ForestRuinsShaders')]
        else:
            command+=['-nullrhi','-run=pythonscript','-script='+str(script)]
        (run/(step+'_command.json')).write_text(json.dumps(command,indent=2),encoding='utf-8')
        report=ROOT/'Saved/ForestRuins'/(('art_preview' if render and args.art_only else step)+'.json')
        previous=report.stat().st_mtime_ns if report.exists() else None
        env=dict(os.environ,PG_FOREST_RUN=str(run))
        if render and args.art_only:env['PG_FOREST_ART_ONLY']='1'
        with (run/(step+'_stdout.log')).open('w',encoding='utf-8') as log:
            process=subprocess.Popen(command,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,
                                     creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
            try: code=process.wait(timeout=420 if render else 300)
            except BaseException:
                if process.poll() is None: process.kill(); process.wait(timeout=30)
                raise
        assert code==0,(step,code,str(run))
        assert report.exists() and report.stat().st_mtime_ns!=previous,('Missing fresh report',step,str(run))
        result=json.loads(report.read_text(encoding='utf-8'))
        assert result['status']=='PASS',(step,result.get('error'),str(run))
        if render:
            sys.path.insert(0,str(ROOT/'Tools/Validation'))
            import RunQA
            log_text=(run/(step+'.log')).read_text(encoding='utf-8-sig',errors='replace')
            problems=([] if args.art_only else RunQA.check_cycle(log_text))+RunQA.unexpected_errors(log_text,'forest')
            if 'Failed to compile Material' in log_text or 'Default Material will be used' in log_text:
                problems.append('Environment material shader compilation failed')
            if RunQA.FATAL.search(log_text):problems.append('Fatal, assertion or ensure in rendered preview log')
            result['log_checks']=dict(full_wave_and_reward_sequence=None if args.art_only else not RunQA.check_cycle(log_text),errors=problems)
            if problems:result['status']='FAIL'
            report.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
            (Path(result['run'])/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
            assert not problems,(step,problems,str(run))
        print(step+': PASS '+str(run),flush=True)

if __name__=='__main__': main()
