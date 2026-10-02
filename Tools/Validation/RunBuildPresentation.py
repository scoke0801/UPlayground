"""Run six authored before/after comparisons and capture cards plus live build HUD."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import struct
import uuid
from RunQA import ROOT,FATAL,read_text,run_process,unexpected_errors

def main():
    version=json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/f'UE_{version}'
    run_id=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_')+uuid.uuid4().hex[:8]
    out=ROOT/'Saved/QA'/(run_id+'_builds');out.mkdir(parents=True)
    print(f'Build presentation evidence: {out}',flush=True)
    screenshots=ROOT/'Saved/Screenshots'
    before={p:p.stat().st_mtime_ns for p in screenshots.rglob('*.png')}
    command=[engine/'Engine/Binaries/Win64/UnrealEditor.exe',ROOT/'UPlayground.uproject','/Game/Maps/RogueArena',
        '-EnablePlugins=PythonScriptPlugin',f'-ExecutePythonScript={ROOT/"Tools/Validation/ProbeBuildKeystonesPIE.py"}',
        '-RenderOffscreen','-immersive','-windowed','-ResX=1280','-ResY=720','-nosplash','-nosound','-unattended','-nop4',
        '-culture=en','-UTF8Output','-PGRunSeed=173001',f'-PGTestProfile=Builds_{run_id}',f'-abslog={out/"presentation.log"}']
    code,timeout=run_process(command,ROOT,out/'presentation.stdout.log',180)
    log=read_text(out/'presentation.log') if (out/'presentation.log').is_file() else ''
    problems=unexpected_errors(log,'builds')
    if code or timeout or FATAL.search(log):problems.append(f'Process failed: code={code}, timeout={timeout}')
    expected=['Bleed_cards','Bleed_before','Bleed_stacks','Bleed_after','Shock_cards','Shock_before','Shock_after','Frenzy_cards','Frenzy_before','Frenzy_after']
    if re.findall(r'PGBuildCapture (\w+)',log)!=expected:problems.append('Missing authored captures')
    pattern=r'PGBuildProbe action=(\w+) targets=(\d+) bleed=(\d+) frenzy=(\d+) shock=(\d+) weak=([\d.]+) refund=(\d+) afterimage=(\d+) cooldown=([\d.]+)'
    samples={r[0]:dict(zip(('targets','bleed','frenzy','shock','weak','refund','afterimage','cooldown'),map(float,r[1:]))) for r in re.findall(pattern,log) if r[0] in expected}
    def check(label,field,predicate):
        if label not in samples or not predicate(samples[label][field]):problems.append(label+' incorrect '+field)
    check('Bleed_stacks','bleed',lambda v:v==2)
    check('Bleed_before','refund',lambda v:v==0)
    check('Bleed_after','refund',lambda v:v==1)
    check('Bleed_after','cooldown',lambda v:5.5<v<6.6)
    check('Shock_before','weak',lambda v:v==0)
    check('Shock_after','weak',lambda v:v>0)
    check('Frenzy_before','frenzy',lambda v:v==10)
    check('Frenzy_after','frenzy',lambda v:v==0)
    check('Frenzy_after','afterimage',lambda v:v==1)
    if 'PGBuildPresentation COMPLETE' not in log:problems.append('Missing completion')
    captures=[]
    for path in sorted(screenshots.rglob('*.png'),key=lambda p:p.stat().st_mtime_ns):
        if before.get(path)!=path.stat().st_mtime_ns:
            dest=out/path.name;shutil.copy2(path,dest)
            if struct.unpack('>II',dest.read_bytes()[16:24])!=(1280,720):problems.append('Wrong capture resolution '+dest.name)
            captures.append(dest.name)
    if len(captures)!=len(expected):problems.append('Expected ten captures')
    report=dict(schema=1,status='FAIL' if problems else 'PASS',assisted=True,direct_input=False,
        synthetic_hits=True,problems=problems,samples=samples,screenshots=captures,labels=expected,
        command=[str(x) for x in command])
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'Build presentation {report["status"]}: {problems}',flush=True)
    return int(bool(problems))

if __name__=='__main__':raise SystemExit(main())
