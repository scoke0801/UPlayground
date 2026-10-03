"""Reward, result and dense-loot UI fixtures. Isolated UI_* profiles only."""
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
    parser.add_argument('--cases', default='reward720,reward1080,rewardWide,result720,result1080,resultWide,pending,pendingRetry,interaction,empty,loot,loot100,lootFar')
    args = parser.parse_args()
    version = json.loads(read_text(ROOT / 'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games' / ('UE_' + version)
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:8]
    out = ROOT / 'Saved/QA' / (run_id + '_reward_loot')
    out.mkdir(parents=True)
    cases = dict(reward720=(1280, 720, 'reward'), reward1080=(1920, 1080, 'reward'), rewardWide=(2560, 1080, 'reward'),
                 result720=(1280, 720, 'result'), result1080=(1920, 1080, 'result'), resultWide=(2560, 1080, 'result'))
    for name in ('pending', 'pendingRetry', 'interaction', 'empty', 'loot', 'loot100', 'lootFar'):
        cases[name] = (1280, 720, name)
    report = dict(status='RUNNING', assisted=True, physical_input=False, cases=[])
    print('Reward and loot evidence: ' + str(out), flush=True)
    for name in args.cases.split(','):
        width, height, mode = cases[name]
        screenshots = ROOT / 'Saved/Screenshots'
        before = {p: p.stat().st_mtime_ns for p in screenshots.rglob('PGReward*.png')}
        command = [engine / 'Engine/Binaries/Win64/UnrealEditor.exe', ROOT / 'UPlayground.uproject', '/Game/Maps/RogueArena',
                   '-game', '-ExecCmds=t.MaxFPS 30,PGRewardProbe ' + mode, '-seconds=20', '-RenderOffscreen', '-windowed', '-ForceRes',
                   f'-ResX={width}', f'-ResY={height}', '-nosplash', '-nosound', '-unattended', '-nop4',
                   '-culture=en', '-PGTestProfile=UI_' + uuid.uuid4().hex[:20], f'-abslog={out / (name + ".log")}']
        code, timeout = run_process(command, ROOT, out / (name + '.stdout.log'), 120)
        path = out / (name + '.log')
        log = read_text(path) if path.is_file() else ''
        problems = unexpected_errors(log, name)
        if name in ('pending', 'pendingRetry'):
            expected = 'LogTemp: Error: Stage 6 failed: 클리어 기록과 전리품을 저장하지 못했습니다. 저장을 다시 시도해 주세요.'
            injected = [line for line in problems if line.endswith(expected)]
            problems = [line for line in problems if not line.endswith(expected)]
            if len(injected) != 1:
                problems.append('Expected exactly one injected victory-save failure')
        if code or timeout or FATAL.search(log):
            problems.append(f'Process failed: code={code} timeout={timeout}')
        if 'PGRewardProbe OPEN mode=' + mode not in log:
            problems.append('Fixture did not open')
        marker = 'interaction' if name == 'interaction' else 'pending' if name == 'pendingRetry' else 'loot' if name.startswith('loot') else 'layout'
        if 'PGRewardProbe CHECK ' + marker + ' complete' not in log or 'PGRewardProbe FAIL' in log:
            problems.append('Required UI checks did not pass')
        captures = []
        for path in sorted(screenshots.rglob('PGReward*.png')):
            if before.get(path) != path.stat().st_mtime_ns:
                target = out / (name + '_' + path.name)
                shutil.copy2(path, target)
                if struct.unpack('>II', target.read_bytes()[16:24]) != (width, height):
                    problems.append('Wrong screenshot resolution: ' + target.name)
                captures.append(target.name)
        if not captures:
            problems.append('No current-run screenshot')
        entry = dict(name=name, status='FAIL' if problems else 'PASS', problems=problems, screenshots=captures, command=[str(a) for a in command])
        report['cases'].append(entry)
        report['status'] = 'FAIL' if any(c['problems'] for c in report['cases']) else 'PASS'
        (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(name + ': ' + entry['status'] + ' ' + str(problems), flush=True)
    return int(report['status'] != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())
