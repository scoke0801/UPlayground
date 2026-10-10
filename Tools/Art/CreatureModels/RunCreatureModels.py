"""Run isolated UE import/configure, reload validation, or rendered gallery.
Usage: <python> Tools/Art/CreatureModels/RunCreatureModels.py --step all
"""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from datetime import datetime,timezone
ROOT=Path(__file__).resolve().parents[3];ART=Path(__file__).parent
OUT=ROOT/'Saved/CreatureModels'
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step',choices=['all','import','configure','validate','preview'],default='all')
    args=parser.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    if args.step in ('all','import'):
        manifest=json.loads((ART/'manifest.json').read_text(encoding='utf-8'))
        for entry in manifest['files']:
            target=ART/entry['path']
            if not target.exists():
                source=Path(manifest['source_root'])/Path(entry['path']).relative_to('Source')
                assert source.is_file(),source
                assert hashlib.sha256(source.read_bytes()).hexdigest()==entry['sha256'],source
                target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
            assert hashlib.sha256(target.read_bytes()).hexdigest()==entry['sha256'],target
    run=OUT/'Runs'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f');run.mkdir(parents=True)
    content=ROOT/'Content/Art/CreatureModels'
    if args.step in ('all','import','configure') and content.exists():shutil.copytree(content,run/'Backup')
    version=json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation']
    engine=Path('C:/Program Files/Epic Games')/('UE_'+version)/'Engine/Binaries/Win64'
    results=[]
    for step in (['import','configure','validate','preview'] if args.step=='all' else [args.step]):
        script=ART/(step.title()+'CreatureModels.py');render=step=='preview'
        report=OUT/(step+'.json');old=report.stat().st_mtime_ns if report.exists() else None
        command=[str(engine/('UnrealEditor.exe' if render else 'UnrealEditor-Cmd.exe')),str(ROOT/'UPlayground.uproject'),'-unattended','-nosound','-nop4','-culture=en','-DisablePlugins=RiderLink','-EnablePlugins=PythonScriptPlugin','-ddc=InstalledNoZenLocalFallback','-ShaderWorkingDir='+str(OUT/'Shaders'),'-abslog='+str(run/(step+'.log'))]
        command+=['-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ExecutePythonScript='+str(script)] if render else ['-nullrhi','-run=pythonscript','-script='+str(script)]
        with (run/(step+'.stdout.log')).open('w',encoding='utf-8') as log:
            process=subprocess.Popen(command,cwd=ROOT,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
            try:code=process.wait(timeout=1800)
            except subprocess.TimeoutExpired:
                process.kill();process.wait();raise
        assert report.exists() and report.stat().st_mtime_ns!=old,('Missing fresh report',step,run)
        result=json.loads(report.read_text())
        engine_log=(run/(step+'.log')).read_text(encoding='utf-8',errors='replace')
        assert not any(marker in engine_log for marker in ('Fatal error:','LogPython: Error','Failed to compile Material')),(step,'Engine errors',run)
        if render:
            for model in ('Griffin','MainPlant','EnemyPlant','EnemyRoot'):
                variant='Brown' if model=='Griffin' else 'V1'
                idle=(OUT/('BP_PG_'+model+'_'+variant+'.png')).read_bytes()
                attack=(OUT/(model+'_Attack.png')).read_bytes()
                assert hashlib.sha256(idle).digest()!=hashlib.sha256(attack).digest(),(model,'Unchanged pose capture')
        results.append(dict(step=step,code=code,status=result['status']))
        (run/'results.json').write_text(json.dumps(results,indent=2))
        assert code==0 and result['status']=='PASS',(step,result.get('error'),run)
        print(step,'PASS',flush=True)
    print(run)
if __name__=='__main__':main()
