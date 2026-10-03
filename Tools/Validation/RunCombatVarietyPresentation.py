"""Collect current-run rendered evidence and mixed-pack observations; fail on missing evidence."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import struct
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, unexpected_errors


def main():
    version = json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Epic Games'/f'UE_{version}'
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:8]
    out = ROOT/'Saved/QA'/(run_id+'_combat_variety')
    out.mkdir(parents=True)
    print(f'Combat variety evidence: {out}',flush=True)
    command = [engine/'Engine/Binaries/Win64/UnrealEditor.exe',ROOT/'UPlayground.uproject',
        '-EnablePlugins=PythonScriptPlugin',f'-ExecutePythonScript={ROOT / "Tools/Validation/ProbeCombatVarietyPIE.py"}',
        '-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-nosplash','-nosound','-unattended','-nop4',
        '-culture=en','-UTF8Output','-PGRunSeed=173001',f'-PGTestProfile=Variety_{run_id}',f'-PGVarietyEvidence={out}',f'-abslog={out / "presentation.log"}']
    code,timeout = run_process(command,ROOT,out/'presentation.stdout.log',240)
    log = read_text(out/'presentation.log') if (out/'presentation.log').is_file() else ''
    problems = unexpected_errors(log,'combat_variety')
    if code or timeout or FATAL.search(log): problems.append(f'Process failed: code={code}, timeout={timeout}')
    samples = [json.loads(row) for row in re.findall(r'PGVarietyProbe sample=(\{[^\n]+\})',log)]
    expected = [(sid,state) for sid in (15111,15112,15113,15114,15115,15116,15109) for state in ('windup','recovery')]
    if [(r['skill'],r['state']) for r in samples] != expected: problems.append('Missing or duplicate new-skill observations')
    if any(not r['active'] or r['recovering'] != (r['state']=='recovery') for r in samples): problems.append('Incorrect captured attack state')
    completions = re.findall(r'PGVarietyProbe COMPLETE (\{[^\n]+\})',log)
    mixed = json.loads(completions[0]) if len(completions)==1 else {}
    if mixed.get('participating_roles') != list(range(15101,15106)) or mixed.get('max_concurrent',99)>3:
        problems.append('Missing mixed-pack budget and participation evidence')
    captures = [f'{sid}_{state}.png' for sid,state in expected]
    for name in captures:
        path = out/name
        if not path.is_file(): problems.append('Missing screenshot: '+name)
        elif struct.unpack('>II',path.read_bytes()[16:24]) != (1280,720): problems.append('Incorrect capture dimensions: '+name)
    report=dict(schema=1,status='FAIL' if problems else 'PASS',assisted=True,direct_input=False,
        problems=problems,samples=samples,mixed=mixed,screenshots=captures,command=[str(a) for a in command])
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'Combat variety {report["status"]}: {problems}',flush=True)
    return int(bool(problems))


if __name__=='__main__': raise SystemExit(main())
