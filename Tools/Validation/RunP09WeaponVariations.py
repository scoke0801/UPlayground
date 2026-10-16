"""Transactional authoring, fresh-process reload and rendered combat validation."""
import argparse
from datetime import datetime, timezone
import json
import os
import shutil
import subprocess
from pathlib import Path
from RunQA import ROOT, run_process, read_text, FATAL
from PlayableCharacterTransaction import restore, write_json


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--step', choices=['inspect', 'apply', 'validate', 'runtime', 'grips', 'dungeon', 'poses'], default='validate')
    parser.add_argument('--render', action='store_true')
    parser.add_argument('--enemy', type=int)
    parser.add_argument('--tempo', action='store_true', help='Record attack timing and intermediate poses')
    parser.add_argument('--film', action='store_true', help='Consecutive fixed-step attack frames (not performance evidence)')
    args = parser.parse_args()
    out = ROOT/'Saved/QA'/('P09Weapons_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f'))
    out.mkdir(parents=True)
    os.environ['PG_P09_WEAPON_RUN'] = str(out)
    os.environ['PG_DUNGEON_QA_DIR'] = str(out)
    version = json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'Epic Games'/('UE_'+version)/'Engine/Binaries/Win64'
    flags = ['-unattended', '-nop4', '-nosound', '-culture=en', '-DisablePlugins=RiderLink',
             '-ddc=InstalledNoZenLocalFallback', '-EnablePlugins=PythonScriptPlugin',
             '-UserDir='+str(out/'User'), '-PGTestProfile='+out.name]
    if args.enemy: flags.append('-PGP09WeaponEnemy='+str(args.enemy))
    if args.tempo: flags.append('-PGEnemyTempoEvidence')
    if args.film:
        assert args.render and args.step == 'runtime'
        flags.extend(['-PGEnemyFilm', '-UseFixedTimeStep', '-FPS=60'])
    steps = [args.step, 'validate'] if args.step in ('apply','grips') else [args.step]
    results = []
    print(out, flush=True)
    for step in steps:
        os.environ['PG_P09_WEAPON_STEP'] = step
        runtime = step in ('runtime','dungeon','poses')
        script = ROOT/'Tools/Validation'/('ProbeP09WeaponDungeon.py' if step=='dungeon' else 'ProbeP09WeaponPoses.py' if step=='poses' else 'ProbeP09Weapons.py' if runtime else 'ConfigureP09Weapons.py')
        extra = (['-ExecutePythonScript='+str(script)] + (['-RenderOffscreen', '-windowed', '-ResX=1280', '-ResY=720'] if args.render else ['-nullrhi'])) if runtime else ['-nullrhi', '-run=pythonscript', '-script='+str(script)]
        exe = engine/('UnrealEditor.exe' if runtime else 'UnrealEditor-Cmd.exe')
        if step=='dungeon':extra.append('-PGDungeonCombat')
        code, timeout = run_process([exe, ROOT/'UPlayground.uproject']+flags+extra+['-abslog='+str(out/(step+'.log'))], ROOT, out/(step+'.stdout.log'), 900)
        log = read_text(out/(step+'.log')) if (out/(step+'.log')).exists() else ''
        marker='PGDungeonCombat PROBE PASS' if step=='dungeon' else 'PGP09Weapons '+step.upper()+' PASS'
        ok = not (code or timeout or FATAL.search(log) or 'LogPython: Error' in log) and marker in log
        if step=='dungeon':
            from P09WeaponRoster import IDS
            ok = ok and log.count('rewards=7 counts=1,2,1,2,1 victory=1')==3
            evidence=json.loads((out/'runtime.json').read_text(encoding='utf-8')) if (out/'runtime.json').exists() else {}
            ok = ok and evidence.get('status')=='PASS' and IDS<=set(evidence.get('p09_weapon_ids',[]))
        results.append(dict(step=step, status='PASS' if ok else 'FAIL', code=code, timeout=timeout))
        print(step+': '+results[-1]['status'], flush=True)
        if not ok:
            if args.step in ('apply','grips') and (out/'transaction.json').exists(): restore(ROOT, out)
            break
    success = all(r['status'] == 'PASS' for r in results)
    if args.film:
        frames = sorted(out.glob('Frame*.png'))
        assert len(frames) >= 30 and [p.name for p in frames] == [f'Frame{i:06d}.png' for i in range(len(frames))]
        ffmpeg = shutil.which('ffmpeg')
        assert ffmpeg
        encoded = subprocess.run([ffmpeg, '-y', '-framerate', '60', '-i', str(out/'Frame%06d.png'),
            '-c:v', 'libx264', '-crf', '18', '-pix_fmt', 'yuv420p', str(out/'continuous.mp4')], capture_output=True)
        (out/'encode.log').write_bytes(encoded.stderr)
        success = success and encoded.returncode == 0
    if success and args.step in ('apply','grips'):
        transaction = json.loads((out/'transaction.json').read_text(encoding='utf-8'))
        transaction['status'] = 'VERIFIED'; write_json(out/'transaction.json', transaction)
    write_json(out/'report.json', dict(status='PASS' if success else 'FAIL', gates=results))
    return not success


if __name__ == '__main__': raise SystemExit(main())
