"""Launch a fresh process against a completed QA profile and capture the restored reward."""
import argparse
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
    parser = argparse.ArgumentParser()
    parser.add_argument('--cycle-report', type=Path, required=True)
    args = parser.parse_args()
    cycle_report = json.loads(read_text(args.cycle_report))
    cycle = next(g for g in cycle_report['gates'] if g['name'] == 'cycle' and g['status'] == 'PASS')
    profile = next(a for a in cycle['command'] if a.startswith('-PGTestProfile='))
    version = json.loads(read_text(ROOT / 'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games' / ('UE_' + version)
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:8]
    out = ROOT / 'Saved/QA' / (run_id + '_loot_result')
    out.mkdir(parents=True)
    screenshots = ROOT / 'Saved/Screenshots'
    before = {p: p.stat().st_mtime_ns for p in screenshots.rglob('*.png')}
    command = [engine / 'Engine/Binaries/Win64/UnrealEditor.exe', ROOT / 'UPlayground.uproject', '/Game/Maps/RogueArena',
               '-game', '-ExecCmds=t.MaxFPS 60,PGProfileStatus,PGStageStatus,Shot SHOWUI', '-seconds=8',
               '-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720', '-nosplash', '-nosound',
               '-unattended', '-nop4', '-culture=en', profile, f'-abslog={out / "presentation.log"}']
    print('Loot result evidence: ' + str(out), flush=True)
    code, timeout = run_process(command, ROOT, out / 'presentation.stdout.log', 90)
    log = read_text(out / 'presentation.log') if (out / 'presentation.log').is_file() else ''
    problems = unexpected_errors(log, 'loot_result')
    if code or timeout or FATAL.search(log):
        problems.append(f'Process failed: code={code} timeout={timeout}')
    if 'PG Stage=6 State=5 ' not in log or 'PGLoot result restored ' not in log:
        problems.append('Missing saved result observation')
    if 'PGLoot boss committed ' in log or 'PGLoot rolled ' in log:
        problems.append('Reload unexpectedly rerolled or recommitted loot')
    source_log = read_text(args.cycle_report.parent / cycle['log'])
    source_reward = re.findall(r'PGLoot boss committed item=(\d+) guid=([0-9A-Fa-f]+) wins=(\d+)', source_log)
    restored = re.findall(r'PGLootProfile seed=\d+ run=\w+ claims=\d+ boss=(\d+) guid=([0-9A-Fa-f]+) wins=(\d+) ended=1 checkpoint=6', log)
    if len(source_reward) != 1 or restored != source_reward:
        problems.append('Fresh process did not preserve the exact saved reward identity and victory count')
    captures = []
    for path in sorted(screenshots.rglob('*.png')):
        if before.get(path) != path.stat().st_mtime_ns:
            target = out / path.name
            shutil.copy2(path, target)
            if struct.unpack('>II', target.read_bytes()[16:24]) != (1280, 720):
                problems.append('Wrong screenshot resolution: ' + target.name)
            captures.append(target.name)
    if len(captures) != 1:
        problems.append(f'Expected one result screenshot, got {len(captures)}')
    report = dict(status='FAIL' if problems else 'PASS', assisted=True, direct_input=False, problems=problems,
                  screenshots=captures, source_report=str(args.cycle_report), command=[str(a) for a in command])
    (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('Loot result ' + report['status'] + ': ' + str(problems), flush=True)
    return int(bool(problems))


if __name__ == '__main__':
    raise SystemExit(main())
