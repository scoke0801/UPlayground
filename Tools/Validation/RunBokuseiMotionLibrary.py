"""Run the Bokusei fixture sequentially with bounded, hidden Unreal processes."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, default=Path('C:/Program Files/Epic Games/UE_5.8'))
    parser.add_argument('--step', choices=['all','configure','preview','validate','inspect'], default='all')
    parser.add_argument('--wait-editor-seconds', type=int, default=0)
    args = parser.parse_args()
    def wait_for_editor():
        deadline=time.monotonic()+args.wait_editor_seconds
        while True:
            active = subprocess.run(['powershell','-NoProfile','-Command',
                'Get-Process UnrealEditor* -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id'],
                capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
            if not active.stdout.strip():
                return
            if time.monotonic()>=deadline:
                raise RuntimeError('Unreal editor is still active: '+active.stdout.strip())
            time.sleep(3)
    out = ROOT/'Saved/BokuseiMotionLibrary/Runs'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    out.mkdir(parents=True)
    report = dict(status='RUNNING',run=str(out),steps=[])
    plan = [('configure','ConfigureBokuseiMotionLibrary.py','configure.json'),
            ('preview','PreviewBokuseiMotionLibrary.py','preview.json'),
            ('validate','ValidateBokuseiMotionLibrary.py','validation.json')]
    if args.step=='inspect':
        plan=[('inspect','InspectBokuseiRootMotion.py','root_motion_probe.json')]
    try:
        for step, script, result_file in plan:
            if args.step not in ('all',step):
                continue
            wait_for_editor()
            render = step == 'preview'
            command = [str(args.engine/('Engine/Binaries/Win64/UnrealEditor.exe' if render else 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe')),
                       str(ROOT/'UPlayground.uproject'), '-unattended','-nosound','-nosplash','-nop4','-culture=en',
                       '-DisablePlugins=RiderLink','-EnablePlugins=PythonScriptPlugin','-ddc=InstalledNoZenLocalFallback',
                       '-abslog='+str(out/(step+'.log'))]
            path = str(ROOT/'Tools/Validation'/script)
            command += ['-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720','-ExecutePythonScript='+path] if render else ['-nullrhi','-run=pythonscript','-script='+path]
            (out/(step+'_command.json')).write_text(json.dumps(command,indent=2),encoding='utf-8')
            latest = ROOT/'Saved/BokuseiMotionLibrary'/result_file
            previous = latest.stat().st_mtime_ns if latest.exists() else None
            with (out/(step+'_stdout.log')).open('w',encoding='utf-8') as log:
                proc = subprocess.Popen(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
                                        stdin=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
                try:
                    code = proc.wait(timeout=960)
                except BaseException:
                    if proc.poll() is None:
                        subprocess.run(['taskkill','/PID',str(proc.pid),'/T','/F'],capture_output=True,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
                        proc.wait(timeout=30)
                    raise
            assert code == 0, (step,code)
            assert latest.exists() and latest.stat().st_mtime_ns != previous, 'No fresh '+result_file
            result = json.loads(latest.read_text(encoding='utf-8'))
            assert result['status'] == ('CAPTURED' if render else 'PASS'), result.get('error',result['status'])
            if render:
                assert result['pie_playback']['status'] == 'PASS'
            (out/result_file).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
            report['steps'].append(dict(name=step,status=result['status']))
        report['status'] = 'PASS'
    except BaseException as error:
        report.update(status='FAIL',error=str(error))
        raise
    finally:
        (out/'run.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False,indent=2))


if __name__ == '__main__':
    main()
