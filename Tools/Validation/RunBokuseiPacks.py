"""Run the Bokusei fixture sequentially with bounded, hidden Unreal processes."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import time
import os
import hashlib
import re

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, default=Path('C:/Program Files/Epic Games/UE_5.8'))
    parser.add_argument('--step', choices=['all','prepare','configure','preview','validate'], default='all')
    parser.add_argument('--wait-editor-seconds', type=int, default=0)
    parser.add_argument('--pilot', action='store_true')
    parser.add_argument('--limit', type=int, default=0)
    parser.add_argument('--maps-only', action='store_true',help='Verify gallery changes and all animation file hashes after a completed full validation.')
    parser.add_argument('--allow-quality-capture', action='store_true',
                        help='Allow NullRHI stages beside the isolated fixed-step Inori LOD image capture; never a benchmark or user editor.')
    parser.add_argument('--allow-pack-import', action='store_true',
                        help='Allow a gallery render beside this tool\'s NullRHI pack conversion, reading an already completed pilot report.')
    args = parser.parse_args()
    if args.maps_only and args.step!='validate':parser.error('--maps-only requires --step validate')
    protection=ROOT/'Saved/BokuseiPacks/protected.json'
    if not protection.exists():
        files=[p for p in (ROOT/'Content/Art/ToonTest/Bokusei').rglob('*.uasset')
               if not any(part in ['FrankSlash','GrruzamSword','RPGAnimations'] for part in p.parts)]
        files+=list((ROOT/'Content/Art/ToonTest/Maps/KatanaLibrary').glob('*.umap'))
        files.append(ROOT/'Content/Art/ToonTest/Maps/L_PGToon_Bokusei_MotionTest.umap')
        protection.parent.mkdir(parents=True,exist_ok=True)
        protection.write_text(json.dumps({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},indent=2),encoding='utf-8')
    environment=os.environ.copy()
    environment['PG_BOKUSEI_PACK_PILOT']='1' if args.pilot else '0'
    environment['PG_BOKUSEI_PACK_LIMIT']=str(args.limit)
    environment['PG_BOKUSEI_PACK_MAPS_ONLY']='1' if args.maps_only else '0'
    def wait_for_editor(render):
        deadline=time.monotonic()+args.wait_editor_seconds
        while True:
            active = subprocess.run(['powershell','-NoProfile','-Command',
                'Get-Process UnrealEditor* -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id'],
                capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
            if not active.stdout.strip():
                return
            if (args.allow_quality_capture and not render) or (args.allow_pack_import and render):
                detail=subprocess.run(['powershell','-NoProfile','-Command',
                    'Get-CimInstance Win32_Process -Filter "Name LIKE \'UnrealEditor%\'" | Select-Object ProcessId,CommandLine | ConvertTo-Json -Compress'],
                    capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
                if detail.returncode==0 and detail.stdout.strip():
                    rows=json.loads(detail.stdout)
                    if isinstance(rows,dict):rows=[rows]
                    allowed_script=str(ROOT/'Tools/Validation/CaptureToonLODQuality.py').replace('\\','/').lower()
                    if rows and all(allowed_script in (r.get('CommandLine') or '').replace('\\','/').lower()
                                    and '-UseFixedTimeStep' in r['CommandLine'] and '-RenderOffscreen' in r['CommandLine'] for r in rows):
                        if not render:
                            print('NullRHI import beside isolated fixed-step Inori image capture (not a benchmark).',flush=True)
                            return
                    own_import=str(ROOT/'Tools/Validation/ConfigureBokuseiPacks.py').replace('\\','/').lower()
                    if render and rows and all(own_import in (r.get('CommandLine') or '').replace('\\','/').lower()
                                               and '-nullrhi' in r['CommandLine'].lower() for r in rows):
                        print('Render completed pilot beside isolated NullRHI pack import.',flush=True)
                        return
            if time.monotonic()>=deadline:
                raise RuntimeError('Unreal editor is still active: '+active.stdout.strip())
            time.sleep(3)
    out = ROOT/'Saved/BokuseiPacks/Runs'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    out.mkdir(parents=True)
    report = dict(status='RUNNING',run=str(out),steps=[])
    plan = [('prepare','PrepareBokuseiPacks.py','setup.json'),
            ('configure','ConfigureBokuseiPacks.py','configure.json'),
            ('preview','PreviewBokuseiPacks.py','preview.json'),
            ('validate','ValidateBokuseiPacks.py','validation_maps.json' if args.maps_only else 'validation.json')]
    try:
        for step, script, result_file in plan:
            if args.step not in ('all',step):
                continue
            render = step == 'preview'
            wait_for_editor(render)
            command = [str(args.engine/('Engine/Binaries/Win64/UnrealEditor.exe' if render else 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe')),
                       str(ROOT/'UPlayground.uproject'), '-unattended','-nosound','-nosplash','-nop4','-culture=en',
                       '-DisablePlugins=RiderLink','-EnablePlugins=PythonScriptPlugin','-ddc=InstalledNoZenLocalFallback',
                       '-abslog='+str(out/(step+'.log'))]
            path = str(ROOT/'Tools/Validation'/script)
            command += ['-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720','-ExecutePythonScript='+path] if render else ['-nullrhi','-run=pythonscript','-script='+path]
            (out/(step+'_command.json')).write_text(json.dumps(command,indent=2),encoding='utf-8')
            latest = ROOT/'Saved/BokuseiPacks'/result_file
            previous = latest.stat().st_mtime_ns if latest.exists() else None
            with (out/(step+'_stdout.log')).open('w',encoding='utf-8') as log:
                proc = subprocess.Popen(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
                                        stdin=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW,env=environment)
                try:
                    code = proc.wait(timeout=7200)
                except BaseException:
                    if proc.poll() is None:
                        subprocess.run(['taskkill','/PID',str(proc.pid),'/T','/F'],capture_output=True,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
                        proc.wait(timeout=30)
                    raise
            assert latest.exists() and latest.stat().st_mtime_ns != previous, 'No fresh '+result_file
            result = json.loads(latest.read_text(encoding='utf-8'))
            diagnostics=[]
            if code!=0:
                log_text=(out/(step+'.log')).read_text(encoding='utf-8',errors='replace')
                errors=set(re.findall(r'\b[A-Za-z]\w*: Error: (.*)',log_text))
                recovered={'Import mesh have some infinite value in the data.'}
                diagnostics=sorted(errors & recovered)
                # UE explicitly replaces non-finite FBX values with finite defaults.
                # Keep the diagnostic visible; final persisted pose/skin checks are
                # still required and this is not an assertion of source fidelity.
                if diagnostics:
                    (ROOT/'Saved/BokuseiPacks/import_diagnostics.json').write_text(json.dumps(dict(
                        status='SOURCE_REQUIRES_REVIEW',run=str(out),messages=diagnostics),indent=2),encoding='utf-8')
                assert code==1 and errors and errors<=recovered and result['status']=='PASS',(step,code,errors)
                result['recovered_fbx_diagnostics']=diagnostics
            assert result['status'] == ('CAPTURED' if render else 'PASS'), result.get('error',result['status'])
            if render:
                assert result['pie_playback']['status'] == 'PASS'
            (out/result_file).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
            report['steps'].append(dict(name=step,status=result['status'],recovered_fbx_diagnostics=diagnostics))
        report['status'] = 'PASS'
    except BaseException as error:
        report.update(status='FAIL',error=str(error))
        raise
    finally:
        (out/'run.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False,indent=2))


if __name__ == '__main__':
    main()
