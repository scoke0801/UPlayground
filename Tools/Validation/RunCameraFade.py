"""Build, transactionally apply/reload camera fade, and run camera regression tests."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
from RunQA import ROOT, run_process, read_text, FATAL
from PlayableCharacterTransaction import restore, write_json

out = ROOT/'Saved/QA/CameraFade'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')
out.mkdir(parents=True)
os.environ['PG_CAMERA_FADE_RUN'] = str(out)
engine = Path('C:/Program Files/Epic Games/UE_5.8/Engine')
gates = []
def run(name, command, timeout=600):
    print(name, flush=True)
    code, expired = run_process(command, ROOT, out/(name+'.log'), timeout)
    log = read_text(out/(name+'.log'))
    assert code == 0 and not expired and not FATAL.search(log) and 'LogPython: Error' not in log, (name, code, expired)
    assert 'Failed to compile Material' not in log and 'LogShaderCompilers: Error' not in log, name
    gates.append(name)

try:
    run('build', [engine/'Build/BatchFiles/Build.bat', 'UPlaygroundEditor', 'Win64', 'Development',
                 ROOT/'UPlayground.uproject', '-WaitMutex', '-NoHotReloadFromIDE'], 1200)
    common = [engine/'Binaries/Win64/UnrealEditor-Cmd.exe', ROOT/'UPlayground.uproject', '-unattended',
              '-nop4', '-nosound', '-culture=en', '-DisablePlugins=RiderLink', '-ddc=InstalledNoZenLocalFallback',
              '-EnablePlugins=PythonScriptPlugin', '-UserDir='+str(out/'User'), '-PGTestProfile=CameraFade']
    try:
        for step in ['apply', 'verify']:
            run(step, common + ['-nullrhi', '-run=pythonscript', '-script='+str(ROOT/'Tools/Validation/ConfigureCameraFade.py')]
                + (['-PGCameraFadeVerify'] if step == 'verify' else []))
            assert json.loads(read_text(out/(step+'.json')))['status'] == 'PASS'
    except BaseException:
        if (out/'transaction.json').exists(): restore(ROOT, out)
        raise
    tx = json.loads(read_text(out/'transaction.json')); tx['status'] = 'VERIFIED'; write_json(out/'transaction.json', tx)
    run('automation', common + ['-nullrhi', '-ExecCmds=Automation RunTests PG.Camera.',
                              '-TestExit=Automation Test Queue Empty', '-ReportExportPath='+str(out/'Automation')])
    results = json.loads(read_text(out/'Automation/index.json'))
    assert results['failed'] == 0 and results['succeeded'] + results.get('succeededWithWarnings', 0) >= 3
    render = [engine/'Binaries/Win64/UnrealEditor.exe'] + common[1:]
    run('render', render + ['-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720',
                           '-abslog='+str(out/'render-engine.log'),
                           '-ExecutePythonScript='+str(ROOT/'Tools/Validation/PreviewCameraFade.py')])
    render_log = read_text(out/'render-engine.log')
    assert not FATAL.search(render_log) and 'LogPython: Error' not in render_log
    assert 'Failed to compile Material' not in render_log and 'LogShaderCompilers: Error' not in render_log
    assert json.loads(read_text(out/'Presentation/report.json'))['status'] == 'PASS'
    write_json(out/'report.json', dict(status='PASS', gates=gates))
except BaseException as error:
    write_json(out/'report.json', dict(status='FAIL', gates=gates, error=str(error)))
    raise
finally:
    print(out, flush=True)
