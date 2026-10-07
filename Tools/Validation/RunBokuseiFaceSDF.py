"""Reproduce the model-derived Bokusei face SDF with bounded hidden processes."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import re
import shutil

ROOT = Path(__file__).resolve().parents[2]

def run(command, output, timeout=300):
    flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
    (output.with_suffix('.command.json')).write_text(json.dumps([str(v) for v in command], indent=2), encoding='utf-8')
    with output.open('w', encoding='utf-8') as stream:
        environment = dict(os.environ,PG_FACE_SDF_RUN=str(output.parent))
        process = subprocess.Popen([str(v) for v in command], cwd=ROOT, stdout=stream, env=environment,
                                   stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, creationflags=flags)
        try:
            code = process.wait(timeout=timeout)
        except BaseException:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=30)
            raise
    if code:
        raise RuntimeError(f'Process failed ({code}): {output}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step', choices=['all', 'inspect', 'bake', 'configure', 'preview'], default='all')
    parser.add_argument('--engine', type=Path, default=Path('C:/Program Files/Epic Games/UE_5.8'))
    parser.add_argument('--blender', type=Path, default=Path('C:/Program Files/Blender Foundation/Blender 5.2/blender.exe'))
    parser.add_argument('--apply', action='store_true', help='Apply the saved SDF face material to Bokusei')
    parser.add_argument('--input-only',action='store_true',help='Reload and validate PIE without repeating the angular render sweep')
    args = parser.parse_args()
    output = ROOT/'Saved/BokuseiFaceSDF/Runs'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    output.mkdir(parents=True)
    report = dict(status='RUNNING', run=str(output), apply_requested=args.apply, steps=[])
    try:
        for step in ['inspect', 'bake', 'configure', 'preview']:
            if args.step not in ['all', step]:
                continue
            latest = {'inspect':ROOT/'Saved/BokuseiFaceSDF/Model/model.json',
                      'bake':ROOT/'Tools/Art/ToonTest/BokuseiFaceSDF/bake.json',
                      'configure':ROOT/'Saved/BokuseiFaceSDF/configure.json',
                      'preview':ROOT/'Saved/BokuseiFaceSDF/preview.json'}[step]
            previous = latest.stat().st_mtime_ns if latest.exists() else None
            if step == 'bake':
                command = [args.blender.parent/'5.2/python/bin/python.exe',
                           ROOT/'Tools/Art/ToonTest/BokuseiFaceSDF/BakeBokuseiFaceSDF.py']
            else:
                render = step == 'preview'
                script = ROOT/'Tools/Validation'/({'inspect':'Inspect', 'configure':'Configure', 'preview':'Preview'}[step]+'BokuseiFaceSDF.py')
                command = [args.engine/'Engine/Binaries/Win64'/('UnrealEditor.exe' if render else 'UnrealEditor-Cmd.exe'),
                           ROOT/'UPlayground.uproject', '-unattended', '-nosound', '-nosplash', '-nop4', '-culture=en',
                           '-DisablePlugins=RiderLink', '-EnablePlugins=PythonScriptPlugin', '-Multiprocess',
                           '-ddc=InstalledNoZenLocalFallback', '-ShaderWorkingDir='+str(ROOT/'Intermediate/BokuseiFaceSDFShaders'),
                           '-abslog='+str(output/(step+'.engine.log'))]
                command += ['-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1600', '-ResY=900',
                            '-ExecutePythonScript='+str(script)] if render else ['-nullrhi', '-run=pythonscript', '-script='+str(script)]
                if args.apply and step == 'configure':
                    command.append('-PGApplyBokuseiFaceSDF')
                if render:
                    command += ['-PGShadingComparisonProbe','-PGFaceSDFProbe']
                    if args.input_only: command.append('-PGComparisonInputOnly')
            try:
                run(command, output/(step+'.stdout.log'), 360)
                assert latest.exists() and latest.stat().st_mtime_ns != previous,'Missing fresh report: '+str(latest)
                result = json.loads(latest.read_text(encoding='utf-8'))
                assert result['status'] == 'PASS',result.get('error',result)
                if step != 'bake':
                    log = (output/(step+'.engine.log')).read_text(encoding='utf-8-sig',errors='replace')
                    assert not re.search(r'Failed to compile Material|LogShaderCompilers: Error|Fatal error|LogPython: Error',log), 'Engine/shader error: '+str(output)
            except BaseException:
                # The child is closed before replacing a package. Restore only the
                # explicitly targeted face MI; generated candidate assets are isolated.
                backup = output/'backup/original_face.uasset'
                if step == 'configure' and args.apply and backup.is_file():
                    source = ROOT/'Content/Art/ToonTest/Bokusei/Materials/MI_PGToon_Bokusei_Mat_Bokusei_Face.uasset'
                    shutil.copy2(backup,source)
                    report['face_rollback'] = str(backup)
                    latest.unlink(missing_ok=True)
                raise
            report['steps'].append(dict(name=step, status='PASS'))
            if step == 'configure': report['applied'] = result['applied']
            if step == 'preview': report['applied_face_reloaded'] = 'applied_face_reload' in result
            if step == 'configure':
                run([args.engine/'Engine/Binaries/ThirdParty/Python3/Win64/python.exe',
                     ROOT/'Tools/Validation/RunBokuseiShadingComparison.py','--step','configure','--engine',args.engine],
                    output/'comparison.stdout.log',360)
                report['steps'].append(dict(name='comparison',status='PASS'))
        report['status'] = 'PASS'
    except BaseException as error:
        report.update(status='FAIL', error=str(error))
        raise
    finally:
        (output/'run.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(report, ensure_ascii=False, indent=2), flush=True)

if __name__ == '__main__':
    main()
