"""Build, saved-content validation, PG tests and real spatial combat (isolated profile)."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import uuid
from RunQA import ROOT, FATAL, read_text, run_process, check_automation
from HackSlashMetrics import check_spatial_observations


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--skip-build', action='store_true')
    parser.add_argument('--render', action='store_true', help='Also run offscreen SM6 capture of the five skills')
    args = parser.parse_args()
    version = json.loads(read_text(ROOT / 'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games' / ('UE_' + version)
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:6]
    out = ROOT / 'Saved/QA' / (run_id + '_hack_slash_p0')
    out.mkdir(parents=True)
    common = [engine / 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe', ROOT / 'UPlayground.uproject']
    flags = ['-nullrhi', '-unattended', '-nop4', '-nosound', '-culture=en', '-DisablePlugins=RiderLink',
             '-ddc=InstalledNoZenLocalFallback', '-PGTestProfile=HackSlash_' + run_id]
    cases = []
    if not args.skip_build:
        sdk = list((engine / 'Engine/Binaries/ThirdParty/DotNet').glob('*/win-x64/dotnet.exe'))
        assert len(sdk) == 1
        cases.append(('build', [sdk[0], engine / 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll',
                      'UPlaygroundEditor', 'Win64', 'Development', '-Project=' + str(ROOT / 'UPlayground.uproject'),
                      '-WaitMutex', '-NoHotReloadFromIDE', '-Log=' + str(out / 'build.log')], 'Result: Succeeded', 1800))
    for name, extra, marker in [
        ('assets', ['-EnablePlugins=PythonScriptPlugin', '-run=pythonscript', '-script=' + str(ROOT / 'Tools/Validation/ValidateRoguelikeMVP.py')], 'PGHackSlash VALIDATION PASS'),
        ('automation', ['-ExecCmds=Automation RunTests PG.', '-TestExit=Automation Test Queue Empty', '-ReportExportPath=' + str(out / 'Automation')], None),
        ('spatial', ['/Game/Maps/RogueArena', '-game', '-ExecCmds=t.MaxFPS 60,pg.Skill.DebugCast 1,pg.Skill.Observe 1,PGHackSlashProbe'], 'PGHackSlashProbe PASS skills=5 real_spatial=1 contact_injected=0')]:
        cases.append((name, common + extra + flags + ['-abslog=' + str(out / (name + '.log'))], marker, 600))
    if args.render:
        cases.append(('render', common + ['/Game/Maps/RogueArena','-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720',
                      '-PGHackSlashCapture','-ExecCmds=t.MaxFPS 60,PGHackSlashProbe'] + [f for f in flags if f != '-nullrhi'] +
                      ['-abslog=' + str(out / 'render.log')], 'PGHackSlashProbe PASS', 600))
    gates = []
    for name, cmd, marker, timeout in cases:
        code, timed_out = run_process(cmd, ROOT, out / (name + '.stdout.log'), timeout)
        log_path = out / (name + '.log')
        log = read_text(log_path) if log_path.exists() else read_text(out / (name + '.stdout.log'))
        errors = []
        if not log_path.exists():
            errors.append('Missing native log; process startup/build failure preserved in stdout')
        if code or timed_out or FATAL.search(log) or (marker and marker not in log):
            errors.append(f'code={code} timeout={timed_out} missing_marker={bool(marker and marker not in log)}')
        if name == 'automation':
            report = out / 'Automation/index.json'
            if report.exists():
                more, warnings = check_automation(json.loads(read_text(report)),
                    ['PG.HackSlash.ProfileValidation','PG.HackSlash.CastDamageAndProcLimits','PG.HackSlash.SpatialClockAndCancellation','PG.HackSlash.CastObservation'])
                errors.extend(more)
            else:
                errors.append('Missing automation report')
        if name == 'spatial':
            observations = check_spatial_observations(log)
            (out / 'spatial_observations.json').write_text(json.dumps(observations,indent=2),encoding='utf-8')
            errors.extend(observations['errors'])
        gates.append({'name':name, 'status':'FAIL' if errors else 'PASS', 'problems':errors, 'command':[str(v) for v in cmd]})
        print(name + ': ' + gates[-1]['status'], flush=True)
        if errors:
            break
    result = {'status':'FAIL' if any(g['problems'] for g in gates) else 'PASS', 'gates':gates,
              'assisted':True, 'direct_input':False, 'contact_injected':False, 'rendering':args.render,
              'p0_acceptance_complete':False, 'remaining':'Direct play comparisons, montage/FX polish and packaged performance gates.'}
    (out / 'report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(out / 'report.json', flush=True)
    return int(result['status'] != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())
