"""Run isolated UE Python jobs for humanoid locomotion authoring and verification."""
import argparse
import json
import os
from datetime import datetime, timezone
from pathlib import Path
from RunQA import ROOT, run_process, read_text, FATAL
from PlayableCharacterTransaction import restore, write_json

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--step', choices=['inspect', 'apply', 'validate', 'preview'], default='inspect')
    args = parser.parse_args()
    out = ROOT/'Saved/HumanoidLocomotion'
    out.mkdir(parents=True, exist_ok=True)
    base = out
    if args.step=='apply':
        out = base/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
        out.mkdir()
        (base/'latest.txt').write_text(str(out),encoding='utf-8')
    elif args.step in ('validate','preview'): out = Path((base/'latest.txt').read_text(encoding='utf-8'))
    if args.step=='preview':
        out=out/'preview'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
        out.mkdir(parents=True)
        (base/'latest-preview.txt').write_text(str(out),encoding='utf-8')
    os.environ['PG_LOCOMOTION_RUN'] = str(out)
    version = json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'Epic Games'/('UE_'+version)/'Engine/Binaries/Win64'
    script = 'InspectHumanoidLocomotion.py' if args.step=='inspect' else 'ConfigureHumanoidLocomotion.py'
    flags = ['-unattended', '-nop4', '-nosound', '-culture=en', '-DisablePlugins=RiderLink', '-ddc=InstalledNoZenLocalFallback', '-EnablePlugins=PythonScriptPlugin', '-UserDir='+str(out/'User')]
    extra = ['-nullrhi', '-run=pythonscript', '-script='+str(ROOT/'Tools/Validation'/script)]
    if args.step=='validate': extra.append('-PGHumanoidLocomotionValidate')
    executable='UnrealEditor-Cmd.exe'
    if args.step=='preview':
        executable='UnrealEditor.exe'
        extra=['-ExecutePythonScript='+str(ROOT/'Tools/Validation/PreviewHumanoidLocomotion.py'),'-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-PGTestProfile=HumanoidLocomotion_'+out.name]
    steps=[(args.step,extra)]
    if args.step=='apply': steps.append(('reload',extra+['-PGHumanoidLocomotionValidate']))
    results=[]
    for step, options in steps:
        code, timeout = run_process([engine/executable, ROOT/'UPlayground.uproject']+flags+options+['-abslog='+str(out/(step+'.log'))], ROOT, out/(step+'.stdout.log'), 600)
        log = read_text(out/(step+'.log'))
        marker='PGHumanoidLocomotion '+('INVENTORY' if step=='inspect' else 'APPLY' if step=='apply' else 'PREVIEW' if step=='preview' else 'VALIDATION')+' PASS'
        failed = bool(code or timeout or FATAL.search(log) or 'LogPython: Error' in log or marker not in log)
        result=dict(step=step, code=code, timeout=timeout, status='FAIL' if failed else 'PASS', output=str(out))
        results.append(result);print(json.dumps(result,indent=2),flush=True)
        if failed:
            if args.step=='apply' and (out/'transaction.json').exists(): restore(ROOT,out)
            break
    if not failed and args.step=='apply':
        data=json.loads((out/'transaction.json').read_text(encoding='utf-8'));data['status']='VERIFIED';write_json(out/'transaction.json',data)
    write_json(out/(args.step+'-report.json'),dict(status='FAIL' if failed else 'PASS',gates=results))
    return failed

if __name__=='__main__': raise SystemExit(main())
