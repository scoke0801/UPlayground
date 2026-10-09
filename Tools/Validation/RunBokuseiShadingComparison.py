"""Build or verify the Bokusei staged fixture with bounded, hidden UE processes."""
import argparse
import json
import os
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step', choices=['all', 'configure', 'guests', 'preview'], default='all')
    parser.add_argument('--engine', type=Path, default=Path('C:/Program Files/Epic Games/UE_5.8'))
    parser.add_argument('--input-only', action='store_true', help='Skip editor screenshots and verify PIE camera input only')
    args = parser.parse_args()
    output = ROOT/'Saved/BokuseiShadingComparison/Runs'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    output.mkdir(parents=True)
    flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
    report = dict(status='RUNNING', run=str(output), steps=[])
    try:
        for step in (['guests'] if args.step == 'guests' else ['configure', 'preview']):
            if args.step not in ['all', step]:
                continue
            if step in ['configure', 'guests']:
                subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                                str(ROOT/'Tools/Art/ToonTest/BokuseiShadingComparison/BuildLabels.ps1')],
                               check=True, timeout=30, creationflags=flags)
            render = step == 'preview'
            script = ROOT/'Tools/Validation'/('PreviewBokuseiShadingComparison.py' if render else 'ConfigureBokuseiShadingComparison.py')
            if step == 'guests':
                script = ROOT/'Tools/Validation/ConfigureBokuseiGuests.py'
            command = [str(args.engine/'Engine/Binaries/Win64'/('UnrealEditor.exe' if render else 'UnrealEditor-Cmd.exe')),
                       str(ROOT/'UPlayground.uproject'), '-unattended', '-nosound', '-nosplash', '-nop4', '-culture=en',
                       '-DisablePlugins=RiderLink', '-EnablePlugins=PythonScriptPlugin', '-Multiprocess',
                       '-ddc=InstalledNoZenLocalFallback', '-ShaderWorkingDir='+str(ROOT/'Intermediate/BokuseiComparisonShaders'),
                       '-abslog='+str(output/(step+'.log'))]
            command += ['-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1600', '-ResY=900', '-ExecutePythonScript='+str(script)] if render else ['-nullrhi', '-run=pythonscript', '-script='+str(script)]
            if render: command.append('-PGShadingComparisonProbe')
            if render and args.input_only: command.append('-PGComparisonInputOnly')
            (output/(step+'_command.json')).write_text(json.dumps(command, indent=2), encoding='utf-8')
            latest = ROOT/'Saved/BokuseiShadingComparison'/(step+'.json')
            previous = latest.stat().st_mtime_ns if latest.exists() else None
            with (output/(step+'_stdout.log')).open('w', encoding='utf-8') as log:
                process = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                           stdin=subprocess.DEVNULL, creationflags=flags)
                (output/(step+'_pid.txt')).write_text(str(process.pid), encoding='utf-8')
                try:
                    code = process.wait(timeout=300)
                except BaseException:
                    if process.poll() is None:
                        process.kill()
                        process.wait(timeout=30)
                    raise
            assert code == 0, (step, code)
            assert latest.exists() and latest.stat().st_mtime_ns != previous, 'Missing fresh report: '+str(latest)
            result = json.loads(latest.read_text(encoding='utf-8'))
            assert result['status'] == 'PASS', result.get('error', result)
            if render and not args.input_only:
                subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                                str(ROOT/'Tools/Art/ToonTest/BokuseiShadingComparison/MeasureShadows.ps1'),
                                '-PreviewDirectory', result['run']], check=True, timeout=30, creationflags=flags)
                report['shadow_pixels'] = str(Path(result['run'])/'shadow_pixels.json')
            report['steps'].append(dict(name=step, status='PASS', result=str(latest)))
        report['status'] = 'PASS'
    except BaseException as error:
        report.update(status='FAIL', error=str(error))
        raise
    finally:
        (output/'run.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
