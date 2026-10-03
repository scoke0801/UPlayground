"""Run rendered guardian verification and preserve a unique set of screenshots/logs."""
from datetime import datetime, timezone
import json
import os
import re
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, unexpected_errors

def main():
    run_id=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_')+uuid.uuid4().hex[:8]
    out=ROOT/'Saved/QA'/(run_id+'_guardian'); out.mkdir(parents=True)
    version=json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    from pathlib import Path
    engine=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/f'UE_{version}'
    command=[engine/'Engine/Binaries/Win64/UnrealEditor.exe',ROOT/'UPlayground.uproject','-EnablePlugins=PythonScriptPlugin',
        f'-ExecutePythonScript={ROOT/"Tools/Validation/PreviewGuardianPIE.py"}', '-RenderOffscreen','-windowed','-ResX=1280','-ResY=720',
        '-nosplash','-unattended','-nop4','-culture=en','-UTF8Output','-PGRunSeed=173001',
        f'-PGTestProfile=Guardian_{run_id}',f'-PGGuardianOut={out}',f'-abslog={out/"presentation.log"}']
    print(str(out),flush=True)
    code,timed_out=run_process(command,ROOT,out/'presentation.stdout.log',200)
    log=read_text(out/'presentation.log') if (out/'presentation.log').exists() else ''
    problems=unexpected_errors(log,'guardian')
    if code or timed_out or FATAL.search(log):problems.append(f'Process failed: {code}, timeout={timed_out}')
    observed=json.loads(read_text(out/'observations.json')) if (out/'observations.json').exists() else {}
    if observed.get('status')!='PASS':problems.append('Missing passing observations')
    states=['walking','guard','windup','locked','impact','recovery','cancelled','mixed_wave','mixed_wave_late']
    if [r['state'] for r in observed.get('samples',[])]!=states:problems.append('Missing rendered states')
    hits=re.findall(r'PGGuardianProbe action=hit guard=(\d) active=(\d) recovery=(\d) scale=([\d.]+) damage=([\d.]+)',log)
    if len(hits)!=3 or [float(h[3]) for h in hits]!=[.3,1.,1.] or any(float(h[4])<=0 for h in hits):
        problems.append(f'Guard/front/back/recovery damage evidence invalid: {hits}')
    captures=list(out.glob('*.png'))
    if len(captures)!=len(states):problems.append(f'Expected {len(states)} screenshots, got {len(captures)}')
    if any(not (out/(state+'.png')).is_file() for state in states):problems.append('Named screenshots missing')
    report=dict(status='FAIL' if problems else 'PASS',problems=problems,observations=observed,hits=hits,command=[str(x) for x in command])
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(dict(status=report['status'],problems=problems,out=str(out)),ensure_ascii=False),flush=True)
    return int(bool(problems))

if __name__=='__main__':raise SystemExit(main())
