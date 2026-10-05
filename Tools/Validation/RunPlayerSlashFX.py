"""Saved FX reload, native regressions and SM6 combat captures in an isolated profile.
Build UPlaygroundEditor Development first. --apply runs the backed-up FX-only migration.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import struct
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, check_automation


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--render-only', action='store_true', help='Capture saved assets without repeating reload/automation gates')
    parser.add_argument('--niagara', action='store_true', help='Configure/validate the Niagara implementation')
    parser.add_argument('--all-phases', action='store_true', help='Capture every swing, including a second run without targets')
    parser.add_argument('--configuration', choices=('Development', 'DebugGame'), default='Development')
    args = parser.parse_args()
    if args.apply and args.render_only:
        parser.error('--apply and --render-only are mutually exclusive')
    if args.all_phases and not args.niagara:
        parser.error('--all-phases requires --niagara')
    version = json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'Epic Games'/('UE_'+version)
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_')+uuid.uuid4().hex[:6]
    out = ROOT/'Saved/QA'/(run_id+('_player_niagara_fx' if args.niagara else '_player_slash_fx'))
    out.mkdir(parents=True)
    (out/'Temp').mkdir()
    # Turnkey launches Build.bat during editor startup; isolate its temporary lock.
    os.environ['TMP'] = os.environ['TEMP'] = str(out/'Temp')
    editor = 'UnrealEditor-Win64-DebugGame-Cmd.exe' if args.configuration == 'DebugGame' else 'UnrealEditor-Cmd.exe'
    common = [engine/'Engine/Binaries/Win64'/editor, ROOT/'UPlayground.uproject']
    flags = ['-unattended', '-nop4', '-nosound', '-culture=en', '-DisablePlugins=RiderLink',
             '-ddc=InstalledNoZenLocalFallback', '-PGTestProfile=HackSlash_FX_'+run_id,
             '-UserDir='+str(out/'User')]
    cases = []
    implementation = 'PlayerNiagara' if args.niagara else 'PlayerSlashFX'
    scripts = [] if args.render_only else ([('apply', 'Configure'+implementation, 'PG'+implementation+' APPLY PASS')] if args.apply else []) + [
            ('reload', 'Validate'+implementation, 'PG'+implementation+' VALIDATION PASS')]
    for name, script, marker in scripts:
        cases.append((name, ['-nullrhi', '-EnablePlugins=PythonScriptPlugin', '-run=pythonscript',
                            '-script='+str(ROOT/'Tools/Validation'/(script+'.py'))], marker))
    if not args.render_only:
        cases.append(('automation', ['-nullrhi', '-ExecCmds=Automation RunTests PG.',
                      '-TestExit=Automation Test Queue Empty', '-ReportExportPath='+str(out/'Automation')], None))
    render_cases = [('p0_render', False), ('p1_render', True)]
    if args.all_phases:
        render_cases += [('p0_miss_render', False), ('p1_miss_render', True)]
    for name, p1 in render_cases:
        extra = ['/Game/Maps/RogueArena', '-game', '-RenderOffscreen', '-windowed', '-ForceRes',
                 '-ResX=1280', '-ResY=720', '-PGHackSlashCapture',
                 '-ExecCmds=t.MaxFPS 60,PGHackSlashProbe']
        if p1:
            extra.append('-PGHackSlashP1Probe')
        if args.niagara:
            extra.append('-PGNiagaraSlashProbe')
        if args.all_phases:
            # Compare the same phase age even when compilation or image readback
            # stalls a render frame; unconstrained runs remain in prior reports.
            extra += ['-PGSwingFXProbe', '-UseFixedTimeStep', '-FPS=60']
        if '_miss_' in name:
            extra.append('-PGSwingFXMiss')
        cases.append((name, extra, 'PGHackSlashProbe PASS skills='+('3 p1=1' if p1 else '5')))
    gates = []
    print(out, flush=True)
    for name, extra, marker in cases:
        command = common+extra+flags+['-abslog='+str(out/(name+'.log'))]
        code, timeout = run_process(command, ROOT, out/(name+'.stdout.log'), 600)
        log = read_text(out/(name+'.log')) if (out/(name+'.log')).exists() else ''
        errors = []
        if code or timeout or FATAL.search(log) or (marker and marker not in log):
            errors.append(f'code={code} timeout={timeout} missing_marker={bool(marker and marker not in log)}')
        if re.search(r'Failed to compile Material|LogShaderCompilers: Error|LogPython: Error', log):
            errors.append('Shader or Python error; inspect log')
        if name.endswith('_render'):
            for skill in ([110, 113, 114] if name.startswith('p1') else [100, 101, 102, 111, 112]):
                suffix = '_Miss' if '_miss_' in name else ''
                path = out/'User/Saved/QA/HackSlashP0'/f'Skill_{skill}{suffix}.png'
                if not path.exists() or struct.unpack('>II', path.read_bytes()[16:24]) != (1280, 720):
                    errors.append(f'Missing or incorrect resolution screenshot: {skill}')
                if args.niagara and f'PGPlayerNiagara Skill={skill} System=' not in log:
                    errors.append(f'Missing simulated Niagara system at hit: {skill}')
                if args.niagara and f'PGPlayerNiagara Cleanup Skill={skill} PASS' not in log:
                    errors.append(f'Missing Niagara cleanup verification: {skill}')
                if args.all_phases:
                    phase_count = {110: 5, 111: 4, 112: 6, 113: 5}.get(skill, 1)
                    for phase in range(phase_count):
                        marker = f'PGPlayerSwingFX Skill={skill} Phase={phase} Miss={int(bool(suffix))} PASS'
                        if marker not in log:
                            errors.append('Missing swing verification: '+marker)
                        damage_marker = f'PGMotionDamage Skill={skill} Phase={phase} Miss={int(bool(suffix))} PASS'
                        if damage_marker not in log:
                            errors.append('Missing per-stroke damage verification: '+damage_marker)
                        phase_path = path if phase == 0 else path.with_name(f'Skill_{skill}{suffix}_Phase_{phase}.png')
                        if not phase_path.exists() or struct.unpack('>II', phase_path.read_bytes()[16:24]) != (1280, 720):
                            errors.append(f'Missing or incorrect phase screenshot: {skill}/{phase}')
        if name == 'automation':
            report = out/'Automation/index.json'
            if report.exists():
                more, _ = check_automation(json.loads(read_text(report)), ['PG.HackSlash.ProfileValidation',
                    'PG.HackSlash.MotionSwingCues',
                    'PG.HackSlash.SpatialClockAndCancellation', 'PG.HackSlash.ProjectileLifetimeAndSweep'])
                errors.extend(more)
            else:
                errors.append('Missing automation report')
        gates.append(dict(name=name, status='FAIL' if errors else 'PASS', errors=errors,
                          command=[str(c) for c in command]))
        print(name+': '+gates[-1]['status'], flush=True)
        if errors:
            break
    result = dict(status='FAIL' if any(g['errors'] for g in gates) else 'PASS', gates=gates,
                  render_only=args.render_only, screenshot_time_paused=True,
                  all_phases=args.all_phases, includes_misses=args.all_phases,
                  configuration=args.configuration,
                  per_stroke_damage_verified=args.all_phases and not any(g['errors'] for g in gates),
                  fixed_capture_timestep=args.all_phases,
                  direct_play_verified=False, packaged_performance_verified=False)
    (out/'report.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(out/'report.json', flush=True)
    return result['status'] != 'PASS'


if __name__ == '__main__':
    raise SystemExit(main())
