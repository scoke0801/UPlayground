"""Isolated inventory fixtures, Slate key routing checks, and real-game captures.

Uses a fresh UI_* test profile for every process; never touches the player profile.
Run after an editor build with Unreal's bundled Python.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import struct
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, unexpected_errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cases', default='open720,open1080,wide,full,empty,build,failure,interaction,states')
    args = parser.parse_args()
    version = json.loads(read_text(ROOT / 'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games' / ('UE_' + version)
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:8]
    out = ROOT / 'Saved/QA' / (run_id + '_inventory')
    out.mkdir(parents=True)
    cases = dict(open720=(1280, 720, 'open'), open1080=(1920, 1080, 'open'), wide=(2560, 1080, 'open'),
                 full=(1280, 720, 'full'), empty=(1280, 720, 'empty'), build=(1280, 720, 'build'),
                 build1080=(1920,1080,'build'), failure=(1280, 720, 'failure'), interaction=(1280, 720, 'open'), states=(1280, 720, 'open'))
    report = dict(status='RUNNING', assisted=True, physical_input=False, cases=[])
    print('Inventory evidence: ' + str(out), flush=True)
    for name in args.cases.split(','):
        width, height, mode = cases[name]
        screenshots = ROOT / 'Saved/Screenshots'
        before = {p: p.stat().st_mtime_ns for p in screenshots.rglob('*.png')}
        commands = 't.MaxFPS 30,PGInventoryProbe ' + mode
        if name == 'interaction':
            commands += ',PGInventoryProbe verify'
        if name == 'states':
            commands += ',PGInventoryProbe verifyStates'
        profile = 'UI_' + uuid.uuid4().hex[:20]
        command = [engine / 'Engine/Binaries/Win64/UnrealEditor.exe', ROOT / 'UPlayground.uproject', '/Game/Maps/RogueArena',
                   '-game', '-ExecCmds=' + commands, '-seconds=10', '-RenderOffscreen', '-windowed', '-ForceRes',
                   f'-ResX={width}', f'-ResY={height}', '-nosplash', '-nosound', '-unattended', '-nop4',
                   '-culture=en', '-PGTestProfile=' + profile, f'-abslog={out / (name + ".log")}']
        code, timeout = run_process(command, ROOT, out / (name + '.stdout.log'), 120)
        path = out / (name + '.log')
        log = read_text(path) if path.is_file() else ''
        problems = unexpected_errors(log, name)
        if code or timeout or FATAL.search(log):
            problems.append(f'Process failed: code={code} timeout={timeout}')
        if 'PGInventoryProbe OPEN mode=' + mode not in log:
            problems.append('Inventory fixture did not open')
        if name == 'interaction' and 'PGInventoryProbe CHECK failures=0' not in log:
            problems.append('Interaction checks did not pass')
        if name == 'states' and 'PGInventoryProbe STATES failures=0' not in log:
            problems.append('Modal state checks did not pass')
        captures = []
        for image in sorted(screenshots.rglob('PGInventory*.png')):
            if before.get(image) != image.stat().st_mtime_ns:
                target = out / (name + '_' + image.name)
                shutil.copy2(image, target)
                if struct.unpack('>II', target.read_bytes()[16:24]) != (width, height):
                    problems.append('Wrong screenshot dimensions: ' + target.name)
                captures.append(target.name)
        if not captures:
            problems.append('No current-run screenshot')
        entry = dict(name=name, status='FAIL' if problems else 'PASS', problems=problems, screenshots=captures,
                     command=[str(a) for a in command])
        report['cases'].append(entry)
        report['status'] = 'FAIL' if any(c['problems'] for c in report['cases']) else 'PASS'
        (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(name + ': ' + entry['status'] + ' ' + str(problems), flush=True)
    return int(report['status'] != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())
