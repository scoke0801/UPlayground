"""Capture native-resolution temporal LOD comparisons in isolated editor PIE.

Uses normal Shot (not HighResShot), a fixed 30 Hz simulation and per-frame LOD
telemetry. This intentionally slow readback is not a performance benchmark.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from RunToonPerformance import processes

def isolated_pack_import(active):
    """Only an identified NullRHI importer, never another renderer/benchmark."""
    if not active or any(p['name'].lower() != 'unrealeditor-cmd' for p in active):
        return False
    result = subprocess.run(['powershell','-NoProfile','-Command',
        "Get-CimInstance Win32_Process -Filter \"Name = 'UnrealEditor-Cmd.exe'\" | Select-Object ProcessId,CommandLine | ConvertTo-Json -Compress"],
        capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
    if result.returncode or not result.stdout.strip():
        return False
    rows = json.loads(result.stdout)
    rows = rows if isinstance(rows,list) else [rows]
    allowed = str(ROOT/'Tools/Validation/ConfigureBokuseiPacks.py').replace('\\','/').lower()
    return {r['ProcessId'] for r in rows} == {p['pid'] for p in active} and all(
        allowed in (r['CommandLine'] or '').replace('\\','/').lower()
        and '-nullrhi' in (r['CommandLine'] or '').lower() for r in rows)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--height', type=int, choices=[720, 1080], default=720)
    parser.add_argument('--smoke', action='store_true')
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--diagnostic', action='store_true')
    parser.add_argument('--shadow-check', action='store_true')
    parser.add_argument('--invalidation-check', action='store_true')
    parser.add_argument('--allow-pack-import', action='store_true',
                        help='Allow only ConfigureBokuseiPacks.py running with NullRHI; capture is not a benchmark')
    parser.add_argument('--wait-editor-seconds', type=int, default=0,
                        help='Wait for existing sequential jobs to finish; require 10 seconds without Unreal processes')
    args = parser.parse_args()
    deadline = time.monotonic()+args.wait_editor_seconds
    quiet_since = None
    while True:
        active = processes()
        if args.allow_pack_import and isolated_pack_import(active):
            print('Concurrent isolated NullRHI pack import: fixed-step image evidence only.',flush=True)
            break
        if not active:
            quiet_since = quiet_since or time.monotonic()
            if args.wait_editor_seconds == 0 or time.monotonic()-quiet_since >= 10:
                break
        else:
            quiet_since = None
        if time.monotonic() >= deadline:
            raise RuntimeError('Existing Unreal job did not finish; no editor is closed: '+str(active))
        time.sleep(3)
    out = ROOT/'Saved/ToonTest/LODQuality'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    out.mkdir(parents=True)
    print(out, flush=True)
    options = vars(args) | {'width': args.height*16//9, 'fps': 30}
    (out/'options.json').write_text(json.dumps(options, indent=2))
    (out/'sources').mkdir()
    for name in ['RunToonLODQuality.py','PrepareToonLODQuality.py','CaptureToonLODQuality.py','ReviewToonLODQuality.py']:
        shutil.copy2(ROOT/'Tools/Validation'/name, out/'sources'/name)
    version = json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation']
    engine = Path('C:/Program Files/Epic Games')/('UE_'+version)
    base = [str(ROOT/'UPlayground.uproject'), '-unattended', '-nosound', '-nosplash', '-nop4', '-culture=en',
            '-DisablePlugins=RiderLink', '-EnablePlugins=PythonScriptPlugin', '-ddc=InstalledNoZenLocalFallback']
    report = {'status': 'RUNNING', 'out': str(out), 'steps': []}
    source_files = [ROOT/'Content/Art/ToonTest/Advanced/Performance/SK_Inori_ToonLOD.uasset',
                    ROOT/'Config/DefaultEngine.ini']
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files}
    try:
        for stage in ['prepare'] + ([] if args.prepare_only else ['capture']):
            render = stage == 'capture'
            command = [str(engine/'Engine/Binaries/Win64'/('UnrealEditor.exe' if render else 'UnrealEditor-Cmd.exe'))]+base
            script = ROOT/'Tools/Validation'/('CaptureToonLODQuality.py' if render else 'PrepareToonLODQuality.py')
            command += ['-abslog='+str(out/(stage+'.log'))]
            command += (['-RenderOffscreen', '-windowed', '-ForceRes', '-ResX='+str(options['width']),
                         '-ResY='+str(args.height), '-UseFixedTimeStep', '-FPS=30', '-ExecutePythonScript='+str(script)]
                        if render else ['-nullrhi', '-run=pythonscript', '-script='+str(script)])
            (out/(stage+'_command.json')).write_text(json.dumps(command, indent=2))
            with (out/(stage+'_stdout.log')).open('w', encoding='utf-8') as log:
                proc = subprocess.Popen(command, cwd=ROOT, env=dict(os.environ, PG_TOON_QUALITY_OUT=str(out)),
                                        stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                        creationflags=subprocess.CREATE_NO_WINDOW)
                try:
                    code = proc.wait(timeout=1800 if render else 300)
                except BaseException:
                    if proc.poll() is None:
                        proc.kill()
                        proc.wait(timeout=30)
                    raise
            result = json.loads((out/(stage+'.json')).read_text())
            assert code == 0 and result['status'] in ['PASS', 'CAPTURED'], result
            report['steps'].append({'stage': stage, 'status': result['status']})
        assert hashes == {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files}
        report.update(status='CAPTURED' if not args.prepare_only else 'PREPARED', preserved_sha256=hashes)
    except BaseException as error:
        report.update(status='FAIL', error=str(error))
        raise
    finally:
        (out/'run.json').write_text(json.dumps(report, indent=2))
        print(json.dumps(report, indent=2), flush=True)

if __name__ == '__main__':
    main()
