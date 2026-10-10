"""Run isolated external combat VFX authoring and validation processes."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from RunQA import run_process, read_text, FATAL

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('step', choices=['inspect', 'build', 'apply', 'validate', 'render'])
    args = parser.parse_args()
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')
    out = ROOT/'Saved/QA/ExternalCombatVFX'/f'{args.step}_{stamp}'
    out.mkdir(parents=True)
    engine = Path('C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe')
    if args.step == 'build':
        command = [str(engine.parents[2]/'Build/BatchFiles/Build.bat'), 'UPlaygroundEditor', 'Win64', 'Development',
                   '-Project='+str(ROOT/'UPlayground.uproject'), '-WaitMutex', '-NoHotReloadFromIDE']
        print(out, flush=True)
        code, timeout = run_process(command, ROOT, out/'build.log', 1200)
        print(f'build: code={code} timeout={timeout}', flush=True)
        return int(bool(code or timeout))
    if args.step == 'render':
        command = [sys.executable, str(Path(__file__).with_name('RunCombatVFX.py')), '--external', '--builds']
        print(out, flush=True)
        code, timeout = run_process(command, ROOT, out/'validation.log', 2400)
        print(read_text(out/'validation.log'), flush=True)
        return int(bool(code or timeout))
    command = [engine, ROOT/'UPlayground.uproject', '-unattended', '-nop4', '-nosound', '-culture=en',
               '-DisablePlugins=RiderLink', '-ddc=InstalledNoZenLocalFallback', '-abslog='+str(out/'editor.log')]
    script = 'InspectExternalVFX.py' if args.step == 'inspect' else 'ConfigureExternalVFX.py'
    command += ['-nullrhi', '-EnablePlugins=PythonScriptPlugin', '-run=pythonscript',
                '-script='+str(Path(__file__).with_name(script))]
    if args.step == 'validate': command.append('-PGExternalVFXValidate')
    print(out, flush=True)
    code, timeout = run_process(command, ROOT, out/'stdout.log', 600)
    log = read_text(out/'editor.log')
    marker = 'PGExternalVFX '+args.step.upper()+' PASS'
    passed = code == 0 and not timeout and not FATAL.search(log) and marker in log and 'LogPython: Error' not in log
    (out/'report.json').write_text(json.dumps(dict(passed=passed, code=code, timeout=timeout), indent=2))
    print(f'{args.step}: {passed}, code={code}, timeout={timeout}', flush=True)
    return int(not passed)

if __name__ == '__main__': raise SystemExit(main())
