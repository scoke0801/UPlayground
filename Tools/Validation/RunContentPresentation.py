"""Run the isolated rendered enemy-pattern probe; fail closed on missing observations/screenshots."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, unexpected_errors


def main():
    version = json.loads(read_text(ROOT / 'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games' / f'UE_{version}'
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:8]
    out = ROOT / 'Saved/QA' / (run_id + '_content')
    out.mkdir(parents=True)
    print(f'Content presentation evidence: {out}', flush=True)
    screenshots = ROOT / 'Saved/Screenshots'
    before = {p: p.stat().st_mtime_ns for p in screenshots.rglob('*.png')}
    command = [engine / 'Engine/Binaries/Win64/UnrealEditor.exe', ROOT / 'UPlayground.uproject',
        '-EnablePlugins=PythonScriptPlugin', f'-ExecutePythonScript={ROOT / "Tools/Validation/ProbeContentRolesPIE.py"}',
        '-RenderOffscreen', '-windowed', '-ResX=1280', '-ResY=720', '-nosplash', '-nosound', '-unattended', '-nop4',
        '-culture=en', '-UTF8Output', '-PGRunSeed=173001', f'-PGTestProfile=Content_{run_id}', f'-abslog={out / "presentation.log"}']
    code, timeout = run_process(command, ROOT, out / 'presentation.stdout.log', 240)
    log = read_text(out / 'presentation.log') if (out / 'presentation.log').is_file() else ''
    problems = unexpected_errors(log, 'content')
    if code or timeout or FATAL.search(log):
        problems.append(f'Process failed: code={code}, timeout={timeout}')
    samples = [json.loads(row) for row in re.findall(r'PGContentProbe sample=(\{[^\n]+\})', log)]
    if [(r['enemy'], r['state']) for r in samples] != [(eid,state) for eid in range(15101,15106) for state in ('windup','recovery')]:
        problems.append('Missing or duplicate authored role observations')
    if any(not r['active'] or r['recovery'] != (r['state'] == 'recovery') for r in samples):
        problems.append('Pattern state does not match the captured phase')
    if 'PGContentProbe COMPLETE ' not in log:
        problems.append('Missing completion')
    captures = []
    for path in sorted(screenshots.rglob('*.png'), key=lambda p: p.stat().st_mtime_ns):
        if before.get(path) != path.stat().st_mtime_ns:
            target = out / path.name
            shutil.copy2(path, target)
            captures.append(target.name)
    if len(captures) != 10:
        problems.append(f'Expected 10 current-run rendered captures, got {len(captures)}')
    report = dict(schema=1, status='FAIL' if problems else 'PASS', assisted=True, direct_input=False,
        culture='en', problems=problems, samples=samples, screenshots=captures, command=[str(a) for a in command])
    (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'Content presentation {report["status"]}: {problems}', flush=True)
    return int(bool(problems))


if __name__ == '__main__':
    raise SystemExit(main())
