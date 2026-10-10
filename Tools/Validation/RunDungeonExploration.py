"""Build and validate P2 discovery without overwriting P1 evidence."""
import argparse
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/QA/ProceduralDungeon/P2'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--step', choices=['build', 'assets', 'automation', 'runtime', 'render', 'combat', 'paths', 'p2', 'p2render'], required=True)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    version = json.loads((ROOT/'UPlayground.uproject').read_text())['EngineAssociation']
    engine = Path('C:/Program Files/Epic Games')/('UE_'+version)/'Engine'
    log = OUT/(args.step+'.log')
    if args.step == 'build':
        cmd = [str(engine/'Build/BatchFiles/Build.bat'), 'UPlaygroundEditor', 'Win64', 'Development',
               str(ROOT/'UPlayground.uproject'), '-WaitMutex', '-NoHotReloadFromIDE']
        marker = 'Result: Succeeded'
    else:
        cmd = [str(engine/'Binaries/Win64/UnrealEditor.exe'), str(ROOT/'UPlayground.uproject'),
               '-unattended', '-nosound', '-nop4', '-culture=ko', '-DisablePlugins=RiderLink',
               '-EnablePlugins=PythonScriptPlugin', '-ddc=InstalledNoZenLocalFallback',
               '-PGTestProfile=DungeonP2Probe', '-UserDir='+str(OUT/'User'), '-abslog='+str(log)]
        if args.step == 'automation':
            cmd += ['-nullrhi', '-ExecCmds=Automation RunTests PG.; Quit',
                    '-TestExit=Automation Test Queue Empty', '-ReportExportPath='+str(OUT/'Automation')]
            marker = 'TEST COMPLETE. EXIT CODE: 0'
        elif args.step == 'assets':
            cmd += ['-nullrhi', '-ExecutePythonScript='+str(ROOT/'Tools/Art/ProceduralDungeon/ConfigureDungeonP2.py')]
            marker = 'PGDungeonP2 ASSETS PASS'
        elif args.step in ('combat','paths'):
            cmd += ['-nullrhi', '-PGDungeonCombat' if args.step=='combat' else '-PGDungeonPreview',
                '-ExecutePythonScript='+str(ROOT/'Tools/Validation'/('ProbeDungeonCombat.py' if args.step=='combat' else 'ProbeProceduralDungeon.py'))]
            marker = 'PGDungeonCombat PROBE PASS' if args.step=='combat' else 'PGDungeon PROBE PASS'
        elif args.step in ('p2','p2render'):
            cmd += ['-PGDungeonCombat','-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeDungeonP2.py')]
            cmd += ['-RenderOffscreen','-PGDungeonP2Render'] if args.step=='p2render' else ['-nullrhi']
            marker = 'PGDungeonP2 PROBE PASS'
        else:
            cmd += ['-PGDungeonPreview', '-ExecutePythonScript='+str(ROOT/'Tools/Validation/ProbeDungeonExploration.py')]
            cmd += ['-RenderOffscreen', '-PGDungeonMapRender'] if args.step == 'render' else ['-nullrhi']
            marker = 'PGDungeonExploration PROBE PASS'
    with (OUT/(args.step+'.stdout.log')).open('w', encoding='utf-8') as stream:
        env=os.environ.copy(); env['PG_DUNGEON_QA_DIR']=str(OUT/args.step)
        proc = subprocess.Popen(cmd, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, env=env,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            code = proc.wait(timeout=1200)
        except subprocess.TimeoutExpired:
            proc.kill(); proc.wait()
            code = -1
    output = (OUT/(args.step+'.stdout.log') if args.step == 'build' else log).read_text(encoding='utf-8', errors='replace')
    rejected = [token for token in ('LogPython: Error', 'Ensure condition failed', 'Fatal error:', 'Failed to compile Material', 'PGDungeonP2 treasure transaction failed', 'PGDungeonP2 treasure probe found no nearby reward') if token in output]
    passed = code == 0 and marker in output and not rejected
    result = dict(step=args.step, exit_code=code, status='PASS' if passed else 'FAIL', marker_found=marker in output)
    if rejected: result['rejected_log_patterns'] = rejected
    (OUT/(args.step+'.result.json')).write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(result, flush=True)
    if not passed: raise SystemExit(1)

if __name__ == '__main__':
    main()
