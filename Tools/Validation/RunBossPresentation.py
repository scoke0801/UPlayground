"""Run the rendered boss pattern/transition/death probe and preserve current-run evidence."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import struct
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, unexpected_errors


def main():
    version = json.loads(read_text(ROOT / 'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games' / f'UE_{version}'
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:8]
    out = ROOT / 'Saved/QA' / (run_id + '_boss')
    out.mkdir(parents=True)
    print(f'Boss presentation evidence: {out}', flush=True)
    screenshots = ROOT / 'Saved/Screenshots'
    before = {p: p.stat().st_mtime_ns for p in screenshots.rglob('*.png')}
    command = [engine / 'Engine/Binaries/Win64/UnrealEditor.exe', ROOT / 'UPlayground.uproject',
        '-EnablePlugins=PythonScriptPlugin', f'-ExecutePythonScript={ROOT / "Tools/Validation/ProbeBossPIE.py"}',
        '-RenderOffscreen', '-windowed', '-ResX=1280', '-ResY=720', '-nosplash', '-nosound', '-unattended', '-nop4',
        '-culture=en', '-UTF8Output', '-PGRunSeed=173001', f'-PGTestProfile=Boss_{run_id}', f'-abslog={out / "presentation.log"}']
    code, timeout = run_process(command, ROOT, out / 'presentation.stdout.log', 240)
    log = read_text(out / 'presentation.log') if (out / 'presentation.log').is_file() else ''
    problems = unexpected_errors(log, 'boss')
    if code or timeout or FATAL.search(log):
        problems.append(f'Process failed: code={code}, timeout={timeout}')
    samples = [json.loads(row) for row in re.findall(r'PGBossProbe sample=(\{[^\n]+\})', log)]
    expected = ['15106_windup','15106_recovery','15107_windup','15107_recovery',
                'transition','15108_windup','15108_recovery','defeated','results','restart']
    if [r['state'] for r in samples] != expected:
        problems.append('Missing or duplicate authored boss observations')
    for row in samples:
        state = row['state']
        phase = 2 if state in ('transition','15108_windup','15108_recovery','defeated','results') else 1
        if row['phase'] != phase or row['transitioning'] != (state == 'transition'):
            problems.append('Incorrect phase: ' + state)
        if row['active'] != ('_' in state) or row['recovering'] != state.endswith('_recovery'):
            problems.append('Incorrect pattern state: ' + state)
    if log.count('PGBoss Phase=2 ') != 1 or log.count('PGBoss Defeated ') != 1:
        problems.append('Transition/defeat must each occur exactly once')
    if 'PGBossProbe COMPLETE ' not in log or 'PGBossProbe death_cleanup PASS' not in log:
        problems.append('Missing completion/cleanup')
    isolated = log.split('PGBossProbe autonomous BEGIN')[0]
    strikes = [(int(a),int(b)) for a,b in re.findall(r'PGPattern Strike skill=(1510[678]) pulse=(\d+)', isolated)]
    if strikes != [(15106,0),(15107,0),(15108,0),(15108,1),(15108,2),(15108,3)]:
        problems.append('Missing, extra, or canceled isolated strikes: ' + str(strikes))
    autonomous = log.split('PGBossProbe autonomous BEGIN')[-1].split('PGBossProbe autonomous END')[0]
    sequence = [int(s) for s in re.findall(r'PGPattern Windup skill=(\d+)', autonomous)]
    if sequence[:3] != [15107,15106,15108]:
        problems.append('AI did not execute its phase-two combination: ' + str(sequence))
    defeat_tail = log.split('PGBoss Defeated ',1)[-1].split('PGBossProbe death_cleanup PASS')[0]
    if 'PGPattern Strike' in defeat_tail:
        problems.append('Attack struck after boss death')
    captures = []
    for path in sorted(screenshots.rglob('*.png'), key=lambda p: p.stat().st_mtime_ns):
        if before.get(path) != path.stat().st_mtime_ns:
            target = out / path.name
            shutil.copy2(path, target)
            if struct.unpack('>II', target.read_bytes()[16:24]) != (1280,720):
                problems.append('Capture is not the requested game window size: ' + target.name)
            captures.append(target.name)
    if len(captures) != len(expected):
        problems.append(f'Expected {len(expected)} current captures, got {len(captures)}')
    report = dict(schema=1, status='FAIL' if problems else 'PASS', assisted=True, direct_input=False,
        culture='en', problems=problems, samples=samples, autonomous_sequence=sequence,
        screenshots=captures, command=[str(a) for a in command])
    (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'Boss presentation {report["status"]}: {problems}', flush=True)
    return int(bool(problems))


if __name__ == '__main__':
    raise SystemExit(main())
