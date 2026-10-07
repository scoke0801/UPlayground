"""Saved-data, PG regression, real-input movement and rendered mobile-combat checks."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, check_automation


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--render-only', action='store_true')
    parser.add_argument('--configuration', choices=('Development', 'DebugGame'), default='DebugGame')
    parser.add_argument('--characters', nargs='+', default=['Bokusei', 'Hwarin', 'Hichi'])
    args = parser.parse_args()
    run = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:6]
    out = ROOT / 'Saved/QA' / (run + '_mobile_combat')
    out.mkdir(parents=True)
    executable = 'UnrealEditor-Win64-DebugGame.exe' if args.configuration == 'DebugGame' else 'UnrealEditor.exe'
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games/UE_5.8/Engine/Binaries/Win64' / executable
    assert engine.is_file(), engine
    common = [engine, ROOT / 'UPlayground.uproject']
    flags = ['-unattended', '-nop4', '-nosound', '-culture=en', '-DisablePlugins=RiderLink', '-ddc=InstalledNoZenLocalFallback']
    cases = []
    if not args.render_only:
        cases.append(('reload', ['-nullrhi', '-EnablePlugins=PythonScriptPlugin', '-run=pythonscript',
            '-script=' + str(ROOT / 'Tools/Validation/ConfigureMobileCombat.py'), '-PGMobileCombatValidate'], 'PGMobileCombat VALIDATION PASS'))
        cases.append(('automation', ['-nullrhi', '-ExecCmds=Automation RunTests PG.', '-TestExit=Automation Test Queue Empty',
            '-ReportExportPath=' + str(out / 'Automation')], None))
        cases.append(('spatial', ['/Game/Maps/RogueArena', '-game', '-nullrhi', '-PGHackSlashP1Probe',
            '-ExecCmds=t.MaxFPS 60,PGHackSlashProbe'], 'PGHackSlashProbe PASS'))
        cases.append(('combo', ['/Game/Maps/RogueArena', '-game', '-RenderOffscreen', '-windowed', '-ResX=1280', '-ResY=720',
            '-PGHackSlashComboProbe', '-ExecCmds=t.MaxFPS 60,PGHackSlashProbe'], 'PGHackSlashProbe PASS combo=100,101,102,100'))
    for identity in args.characters:
        cases.append((identity, ['/Game/Maps/RogueArena', '-game', '-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720',
            '-PGMobileCapture', '-PGMobileCharacter=' + identity, '-ExecCmds=t.MaxFPS 60,PGMobileCombatProbe'], 'PGMobileCombatProbe PASS'))
    print(out, flush=True)
    results = []
    for name, extra, marker in cases:
        profile_prefix = 'HackSlash_' if name in ('spatial', 'combo') else 'MobileCombat_'
        command = common + extra + flags + ['-PGTestProfile=' + profile_prefix + run + '_' + name,
            '-UserDir=' + str(out / name / 'User'), '-abslog=' + str(out / (name + '.log'))]
        code, timeout = run_process(command, ROOT, out / (name + '.stdout.log'), 240)
        log = read_text(out / (name + '.log')) if (out / (name + '.log')).exists() else ''
        errors = []
        if code or timeout or FATAL.search(log) or (marker and marker not in log):
            errors.append(f'code={code} timeout={timeout} missing_marker={bool(marker and marker not in log)}')
        if 'LogPython: Error' in log:
            errors.append('Python error')
        if name == 'automation':
            report = out / 'Automation/index.json'
            if report.exists():
                errors.extend(check_automation(json.loads(read_text(report)), ['PG.HackSlash.SpatialClockAndCancellation'])[0])
            else:
                errors.append('Missing automation report')
        if name in args.characters:
            samples = re.findall(r'PGMobile Skill=(\d+) Distance=([\d.]+) MinSpeed=([\d.]+) LegAngle=([\d.]+) FreezeFrames=(\d+) ExitSpeed=([\d.]+)', log)
            if [int(sample[0]) for sample in samples] != [100, 101, 102, 112, 114]:
                errors.append('Missing walking/leg-motion samples')
            images = out / name / 'User/Saved/QA/MobileCombat'
            if any(not (images / f'Skill{skill}.png').is_file() for skill in (100, 101, 102, 112, 114)):
                errors.append('Missing rendered movement captures')
        results.append(dict(name=name, errors=errors, command=[str(arg) for arg in command]))
        print(name + (': FAIL ' + str(errors) if errors else ': PASS'), flush=True)
        if errors:
            break
    result = dict(status='FAIL' if any(case['errors'] for case in results) else 'PASS', gates=results,
                  enhanced_input_injected=True, manual_keyboard_play=False)
    (out / 'report.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(out / 'report.json', flush=True)
    return result['status'] != 'PASS'


if __name__ == '__main__':
    raise SystemExit(main())
