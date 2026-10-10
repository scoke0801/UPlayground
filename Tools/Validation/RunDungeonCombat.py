"""Run P1 automation, assisted world completion and recovery in an isolated profile."""
import json
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/QA/ProceduralDungeon/P1'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--render', action='store_true')
    parser.add_argument('--only', choices=['automation','runtime','p0_runtime','render'])
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    version = json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation']
    engine = Path('C:/Program Files/Epic Games') / ('UE_'+version) / 'Engine/Binaries/Win64'
    results = []
    cases = [
        ('automation', 'UnrealEditor-Cmd.exe', ['-ExecCmds=Automation RunTests PG.; Quit',
         '-TestExit=Automation Test Queue Empty', '-ReportExportPath='+str(OUT/'Automation')], 'TEST COMPLETE. EXIT CODE: 0'),
        ('runtime', 'UnrealEditor.exe', ['-PGDungeonCombat', '-PGRunSeed=101026',
         '-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeDungeonCombat.py')], 'PGDungeonCombat PROBE PASS'),
        ('p0_runtime', 'UnrealEditor.exe', ['-PGDungeonPreview',
         '-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeProceduralDungeon.py')], 'PGDungeon PROBE PASS')]
    if args.render or args.only == 'render':
        cases.append(('render','UnrealEditor.exe',['-PGDungeonCombat','-culture=ko','-RenderOffscreen',
            '-ExecutePythonScript='+str(ROOT/'Tools/Validation/PreviewDungeonCombat.py')],'PGDungeonCombat RENDER PASS'))
    for name, exe, extra, marker in cases:
        if args.only and args.only != name: continue
        log = OUT/(name+'.log')
        cmd = [str(engine/exe), str(ROOT/'UPlayground.uproject'), '-unattended', '-nosound',
               '-nop4', '-culture=en', '-DisablePlugins=RiderLink', '-EnablePlugins=PythonScriptPlugin',
               '-ddc=InstalledNoZenLocalFallback', '-PGTestProfile=DungeonP1Probe',
               '-UserDir='+str(OUT/'User'), '-abslog='+str(log)] + extra
        if name != 'render': cmd.append('-nullrhi')
        with (OUT/(name+'.stdout.log')).open('w', encoding='utf-8') as stream:
            proc = subprocess.Popen(cmd, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                    creationflags=subprocess.CREATE_NO_WINDOW)
            try:
                code = proc.wait(timeout=1200)
            except subprocess.TimeoutExpired:
                proc.kill(); proc.wait(); raise
        text = log.read_text(encoding='utf-8', errors='replace')
        ok = code == 0 and marker in text and 'LogPython: Error' not in text
        if name == 'runtime':
            ok = ok and text.count('rewards=7 counts=1,2,1,2,1 victory=1') == 3
            ok = ok and 'applied=0 duplicate=0 before=0 after=0 valid=1' in text and 'valid=0' not in text
            ok = ok and 'PGDungeonProbe summons count=2 valid=1' in text
            report = json.loads((OUT/'runtime.json').read_text(encoding='utf-8'))
            report['editor_exit_code'] = code
            report['status'] = 'PASS' if ok else 'FAIL'
            (OUT/'runtime.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        if name == 'render' and (OUT/'Presentation/render.json').exists():
            path = OUT/'Presentation/render.json'
            report = json.loads(path.read_text(encoding='utf-8'))
            report['editor_exit_code'] = code
            report['status'] = 'PASS' if ok else ('FAIL_SHUTDOWN' if report['status']=='PASS' and code else 'FAIL')
            path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        results.append(dict(step=name, exit_code=code, status='PASS' if ok else 'FAIL'))
        (OUT/'result.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
        print(results[-1], flush=True)
        if not ok:
            raise RuntimeError(str(log))

if __name__ == '__main__':
    main()
