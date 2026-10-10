"""Run the autonomous six-role BT probe and retain evidence in a unique QA directory."""
from datetime import datetime, timezone
import argparse
import json
import os
import re
import uuid
from pathlib import Path
from RunQA import ROOT, FATAL, read_text, run_process, unexpected_errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--debug-decisions', action='store_true', help='Capture throttled AI decision reasons')
    args = parser.parse_args()
    version = json.loads(read_text(ROOT / 'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games' / f'UE_{version}'
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:8]
    out = ROOT / 'Saved/QA' / (run_id + '_combat_bt')
    out.mkdir(parents=True)
    print(f'BT probe evidence: {out}', flush=True)
    command = [engine / 'Engine/Binaries/Win64/UnrealEditor.exe', ROOT / 'UPlayground.uproject',
        '-EnablePlugins=PythonScriptPlugin', f'-ExecutePythonScript={ROOT / "Tools/Validation/ProbeCombatBTPIE.py"}',
        '-RenderOffscreen', '-windowed', '-ResX=1280', '-ResY=720', '-nosplash', '-nosound', '-unattended', '-nop4',
        '-DisablePlugins=RiderLink', '-ddc=InstalledNoZenLocalFallback',
        '-culture=en', '-UTF8Output', '-PGRunSeed=173001', f'-PGTestProfile=CombatBT_{run_id}', f'-PGCombatBTEvidence={out}', f'-abslog={out / "probe.log"}']
    if args.debug_decisions:
        command.append('-ExecCmds=pg.AI.DebugDecisions 1')
    code, timeout = run_process(command, ROOT, out / 'probe.stdout.log', 240)
    log = read_text(out / 'probe.log') if (out / 'probe.log').exists() else ''
    problems = unexpected_errors(log, 'combat_bt')
    decision_reasons = sorted(set(re.findall(r'PGCombatDecision .*?reason=(\w+)', log)))
    if args.debug_decisions and 'started' not in decision_reasons:
        problems.append('Missing AI decision execution evidence')
    if code or timeout or FATAL.search(log):
        problems.append(f'Process failure: code={code}, timeout={timeout}')
    completions = re.findall(r'PGCombatBTProbe COMPLETE (\{[^\n]+\})', log)
    sample = json.loads(completions[0]) if len(completions) == 1 else {}
    if not sample or not sample.get('pause_resume') or not sample.get('target_relocation') or not sample.get('boss_phase'):
        problems.append('Missing successful autonomous combat/pause/resume/relocation evidence')
    if sample and (min(sample['starts']) < 1 or min(sample['recoveries']) < 1):
        problems.append('Not every role completed a windup/recovery')
    if sample and sample.get('roles') != list(range(15101, 15107)):
        problems.append('Missing one or more combat roles')
    if sample and sum(sample.get('position_moves', [])) <= 0:
        problems.append('No spatial separation movement observed')
    for name in ('autonomous.png', 'boss_phase_two.png'):
        if not (out / name).is_file():
            problems.append('Missing rendered evidence: ' + name)
    report = dict(schema=2, status='FAIL' if problems else 'PASS', assisted=True, direct_input=False,
                  problems=problems, sample=sample, decision_reasons=decision_reasons, command=[str(a) for a in command])
    (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'BT probe {report["status"]}: {problems}', flush=True)
    return int(bool(problems))


if __name__ == '__main__':
    raise SystemExit(main())
