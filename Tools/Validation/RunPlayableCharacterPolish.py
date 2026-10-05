"""Transactional polish authoring, export, reload and two-pass reproducibility gate."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from PlayableCharacterTransaction import restore, write_json, sha256

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT/'Tools/Validation/Data/PlayableCharacterPolish.json'
FATAL = re.compile(r'Fatal error:|Assertion failed:|Unhandled Exception:|Ensure condition failed:')


def editor(script, output, *, fail_after=0):
    association = json.loads((ROOT/'UPlayground.uproject').read_text(encoding='utf-8-sig'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/('Epic Games/UE_'+association)
    command = [str(engine/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'), str(ROOT/'UPlayground.uproject'),
        '-EnablePlugins=PythonScriptPlugin', '-run=pythonscript', '-script='+str(ROOT/'Tools/Validation'/script),
        '-nullrhi', '-unattended', '-nosound', '-nop4', '-culture=en', '-DisablePlugins=RiderLink', '-Multiprocess',
        '-ddc=InstalledNoZenLocalFallback', '-abslog='+str(output/(Path(script).stem+'.log'))]
    environment = os.environ.copy()
    environment['PG_CHARACTER_RUN'] = str(output)
    environment['PG_CHARACTER_FAIL_AFTER_SAVE'] = str(fail_after)
    write_json(output/(Path(script).stem+'-command.json'), command)
    with (output/(Path(script).stem+'-stdout.log')).open('w', encoding='utf-8') as stream:
        process = subprocess.Popen(command, cwd=ROOT, env=environment, stdout=stream, stderr=subprocess.STDOUT,
                                   stdin=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        write_json(output/'editor-process.json', dict(pid=process.pid, running=True))
        try:
            code = process.wait(timeout=900)
        except BaseException:
            if os.name=='nt':
                subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], capture_output=True,
                               creationflags=subprocess.CREATE_NO_WINDOW, timeout=30)
            else:
                process.kill()
            process.wait(timeout=30)
            raise
        finally:
            if process.poll() is not None:
                write_json(output/'editor-process.json', dict(pid=process.pid, running=False, code=process.returncode))
    log = (output/(Path(script).stem+'.log')).read_text(encoding='utf-8-sig', errors='replace')
    if code or FATAL.search(log) or 'Python script executed successfully' not in log:
        raise RuntimeError(f'{script} failed (code={code}); see {output}')


def configure(output, fail_after=0, grips_only=False):
    try:
        editor('ConfigurePlayableCharacterGrips.py' if grips_only else 'ConfigurePlayableCharacters.py', output, fail_after=fail_after)
        editor('ValidatePlayableCharacters.py', output)
        legacy = json.loads((ROOT/'Saved/PlayableCharacters/validate.json').read_text(encoding='utf-8'))
        assert legacy['status']=='PASS', legacy
        editor('ValidatePlayableCharacterPolish.py', output)
        manifest = output/'transaction.json'
        data = json.loads(manifest.read_text(encoding='utf-8'))
        data['status'] = 'VERIFIED'
        write_json(manifest, data)
        report = json.loads((output/'configure.json').read_text(encoding='utf-8'))
        report['status'] = 'PASS'
        report['semantic_reload'] = True
        write_json(output/'configure.json', report)
        write_json(ROOT/'Saved/PlayableCharacters/configure.json', report)
    except BaseException:
        if (output/'transaction.json').exists():
            restore(ROOT, output)
        # Keep the failure report distinct from an older successful run.
        write_json(ROOT/'Saved/PlayableCharacters/configure.json', dict(status='FAIL', run=str(output)))
        raise


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--step', choices=['configure', 'configure-grips', 'polish-export', 'polish-validate', 'polish-check', 'polish-restore'], required=True)
    parser.add_argument('--restore-run', type=Path, help='Interrupted transaction directory; close its editor process before restoring')
    parser.add_argument('--inject-save-failure', type=int, default=0, help='Fail after N successful saves and restore the full transaction')
    args = parser.parse_args(argv)
    if args.step == 'polish-restore':
        if not args.restore_run: parser.error('polish-restore requires --restore-run')
        directory=args.restore_run.resolve()
        if not directory.is_relative_to(ROOT/'Saved/PlayableCharacters/Runs'):
            parser.error('Restore run must be under Saved/PlayableCharacters/Runs')
        process_file=directory/'editor-process.json'
        if process_file.exists():
            process=json.loads(process_file.read_text(encoding='utf-8'))
            if process['running']:
                # A crashed parent may leave its editor alive. Windows process APIs
                # only query the recorded PID; never terminate a user editor here.
                if os.name=='nt':
                    import ctypes
                    from ctypes import wintypes
                    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
                    kernel.OpenProcess.restype=wintypes.HANDLE
                    kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]
                    kernel.GetExitCodeProcess.argtypes=[wintypes.HANDLE,ctypes.POINTER(wintypes.DWORD)]
                    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
                    handle=kernel.OpenProcess(0x1000,False,process['pid'])
                    if handle:
                        code=wintypes.DWORD()
                        ok=kernel.GetExitCodeProcess(handle,ctypes.byref(code))
                        kernel.CloseHandle(handle)
                        if not ok or code.value==259: parser.error('The transaction editor is still running; close it first')
                    elif ctypes.get_last_error()!=87: parser.error('Cannot verify that the transaction editor exited')
                else:
                    try: os.kill(process['pid'],0)
                    except ProcessLookupError: pass
                    else: parser.error('The transaction editor is still running; close it first')
        data=restore(ROOT,directory)
        lock=ROOT/'Saved/PlayableCharacters/polish.lock'
        if lock.exists() and Path(lock.read_text(encoding='utf-8')).resolve() in (directory,directory.parent): lock.unlink()
        print(data['status'],directory,flush=True)
        return 0
    if args.inject_save_failure and args.step != 'configure':
        parser.error('Failure injection is only available for configure')
    output = ROOT/'Saved/PlayableCharacters/Runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')+'_'+args.step)
    output.mkdir(parents=True)
    lock = ROOT/'Saved/PlayableCharacters/polish.lock'
    # A crashed runner leaves an explicit recovery breadcrumb instead of allowing
    # overlapping writers to invalidate the other run's backups.
    try:
        descriptor = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError:
        print('Polish run already active or interrupted: '+str(lock), flush=True)
        return 1
    os.write(descriptor, str(output).encode('utf-8')); os.close(descriptor)
    try:
        if args.step == 'polish-export':
            editor('ExportPlayableCharacterPolish.py', output)
            data = json.loads((output/'export.json').read_text(encoding='utf-8'))
            if SOURCE.exists(): shutil.copy2(SOURCE, output/'previous-source.json')
            write_json(SOURCE, data)
        elif args.step == 'polish-validate':
            editor('ValidatePlayableCharacterPolish.py', output)
        elif args.step in ('configure', 'configure-grips'):
            configure(output, args.inject_save_failure, grips_only=args.step == 'configure-grips')
        else:
            source_hash=sha256(SOURCE)
            for index in range(2):
                run = output/str(index+1)
                run.mkdir()
                configure(run)
                assert sha256(SOURCE)==source_hash, 'Polish source changed during reproducibility check'
            # Each fresh-process validation compares semantic settings against the
            # same immutable source, not package bytes or a stale global report.
        result = dict(status='PASS', step=args.step, source=str(SOURCE), direct_play=False, visual_acceptance=False)
        write_json(output/'result.json', result)
        print(result, output, flush=True)
        return 0
    except Exception as error:
        result = dict(status='FAIL', error=str(error), step=args.step)
        write_json(output/'result.json', result)
        print(result, output, flush=True)
        return 1
    finally:
        pending = [file for file in output.rglob('transaction.json')
                   if json.loads(file.read_text(encoding='utf-8'))['status'] not in ('VERIFIED','RESTORED')]
        if not pending:
            lock.unlink()
        else:
            print('Recovery required; keeping lock and backups: '+str(pending), flush=True)


if __name__ == '__main__':
    raise SystemExit(main())
