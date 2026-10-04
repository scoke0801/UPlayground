"""Validate saved content, then real GAS/montage combat in a disposable profile."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, unexpected_errors


def main():
    version = json.loads(read_text(ROOT / 'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games' / ('UE_' + version)
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:6]
    out = ROOT / 'Saved/QA' / (run_id + '_player_attacks')
    out.mkdir(parents=True)
    common = [engine / 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe', ROOT / 'UPlayground.uproject',
              '-nullrhi', '-unattended', '-nop4', '-nosound', '-culture=en', '-DisablePlugins=RiderLink',
              '-ddc=InstalledNoZenLocalFallback', '-PGTestProfile=PlayerAttacks_' + run_id]
    problems = []
    gates = []
    cases = [('assets', ['-EnablePlugins=PythonScriptPlugin', '-run=pythonscript',
                        '-script=' + str(ROOT / 'Tools/Validation/ValidateRoguelikeMVP.py')], 'PGRogue VALIDATION PASS'),
             ('combat', ['/Game/Maps/RogueArena', '-game', '-ExecCmds=t.MaxFPS 60,PGPlayerAttackProbe'],
              'PGPlayerAttackProbe PASS combo=1,2,3,1 release=1 idle_reset=1 skills=5 melee_dedup=1 cancel_cleanup=1')]
    for name, args, marker in cases:
        log_path = out / (name + '.log')
        cmd = common[:2] + args + common[2:] + ['-abslog=' + str(log_path)]
        code, timeout = run_process(cmd, ROOT, out / (name + '.stdout.log'), 150)
        log = read_text(log_path) if log_path.exists() else ''
        errors = unexpected_errors(log, name)
        if code or timeout or FATAL.search(log) or marker not in log or 'PGPlayerAttackProbe FAIL' in log:
            errors.append(f'{name}: code={code} timeout={timeout} completion={marker in log}')
        problems.extend(errors)
        gates.append({'name': name, 'status': 'FAIL' if errors else 'PASS', 'problems': errors,
                      'command': [str(a) for a in cmd]})
        print(name + ': ' + gates[-1]['status'], flush=True)
        if errors: break
    result = dict(status='FAIL' if problems else 'PASS', assisted=True, direct_input=False,
                  rendering=False, contact_injected=True, profile_mode='legacy_collision', gates=gates, problems=problems)
    (out / 'report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(str(out / 'report.json'), flush=True)
    return int(bool(problems))


if __name__ == '__main__': raise SystemExit(main())
