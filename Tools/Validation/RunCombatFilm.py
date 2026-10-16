"""Fixed-step consecutive rendered frames; never a real-time performance/playability gate."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid
from RunQA import ROOT, FATAL, read_text, run_process


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--case', choices=['mobile', 'combo', 'impact', 'duel'], required=True)
    parser.add_argument('--label', required=True)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--no-film', action='store_true', help='Metrics-only smoke run')
    parser.add_argument('--skill', action='store_true', help='Start the duel with 112')
    parser.add_argument('--enemies', type=int, choices=(1, 3, 10), default=1)
    parser.add_argument('--bleed', action='store_true', help='Isolated fixed bleed perk fixture, not equipment validation')
    parser.add_argument('--mixed', action='store_true', help='Replace the first infantry with elite spear 15705')
    parser.add_argument('--loop', action='store_true', help='Three basic attacks then 112 when ready')
    parser.add_argument('--equipment', action='store_true', help='Use saved equipment/reward/skill transactions instead of injected perks')
    parser.add_argument('--character', choices=('Bokusei', 'Hwarin', 'Hichi'), default='Bokusei')
    parser.add_argument('--critical', action='store_true', help='Deterministic player critical fixture; enemy critical remains zero')
    parser.add_argument('--two-packs', action='store_true')
    parser.add_argument('--observe', type=float, default=10, help='Pre-combat observation seconds (0..10)')
    parser.add_argument('--headless', action='store_true', help='No renderer; requires --no-film')
    parser.add_argument('--close', action='store_true', help='Use a narrow FOV for pose inspection')
    args = parser.parse_args()
    if args.headless and not args.no_film: parser.error('--headless requires --no-film')
    if args.critical and args.case != 'duel': parser.error('--critical requires --case duel')
    if not 0 <= args.observe <= 10: parser.error('--observe must be 0..10')
    if args.two_packs and not args.loop: parser.error('--two-packs requires --loop')
    ffmpeg = shutil.which('ffmpeg')
    assert ffmpeg, 'ffmpeg required'
    out = ROOT / 'Saved/QA' / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:6] + '_film_' + args.label + '_' + args.case)
    out.mkdir(parents=True)
    mobile = args.case == 'mobile'
    probe = 'PGMobileCombatProbe' if mobile else 'PGFocusedCombatProbe' if args.case == 'duel' else 'PGHackSlashProbe'
    version = json.loads(read_text(ROOT / 'UPlayground.uproject'))['EngineAssociation']
    exe = args.executable or Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / f'Epic Games/UE_{version}/Engine/Binaries/Win64/UnrealEditor.exe'
    command = [exe] + ([] if args.executable else [ROOT / 'UPlayground.uproject']) + [
        '/Game/Maps/RogueArena', '-game', '-RenderOffscreen', '-windowed',
        '-ForceRes', '-ResX=1280', '-ResY=720', '-nosound', '-unattended', '-nop4',
        '-DisablePlugins=RiderLink', '-ddc=InstalledNoZenLocalFallback',
        '-PGMobileCharacter=' + args.character, '-PGCombatVFXFixedAim',
        '-PGRunSeed=173001', '-PGFocusedEnemies=' + str(args.enemies),
        '-PGTestProfile=' + ('MobileCombat_' if mobile else 'FocusedCombat_' if args.case == 'duel' else 'HackSlash_') + out.name,
        '-UserDir=' + str(out / 'User'), '-abslog=' + str(out / 'runtime.log'),
        '-PGFocusedObserve=' + str(args.observe),
        '-ExecCmds=t.MaxFPS 60,pg.Skill.Observe 1,pg.Input.Trace 1,r.MotionBlurQuality 0,' + ('fov 35,' if args.close else '') + probe]
    if args.equipment: command.append('-PGFocusedEquipment')
    command.append('-PGFocusedCharacter=' + args.character)
    if args.critical: command.append('-PGFocusedCritical')
    if args.two_packs: command.append('-PGFocusedTwoPacks')
    if args.bleed: command.append('-PGFocusedBleed')
    if args.mixed: command.append('-PGFocusedMixed')
    if args.loop: command.append('-PGFocusedLoop')
    if args.headless: command[command.index('-RenderOffscreen')]='-nullrhi'
    if args.case == 'combo': command.append('-PGHackSlashComboProbe')
    if args.skill:
        assert args.case == 'duel'
        command.append('-PGFocusedSkill')
    if not args.no_film: command.append('-PGCombatFilm')
    if args.case == 'impact': command.extend(['-PGFeedbackEnemy=15701', '-PGHackSlashMotionTrace'])
    print(out, flush=True)
    code, timeout = run_process(command, ROOT, out / 'stdout.log', 900)
    log = read_text(out / 'runtime.log')
    frames = sorted((out / 'User/Saved/QA/CombatFilm').glob('Frame*.png'))
    errors = []
    if code or timeout or FATAL.search(log) or probe + ' PASS' not in log:
        errors.append(f'Runtime failure: exit={code}, timeout={timeout}')
    if not args.no_film and (len(frames) < 60 or [p.name for p in frames] != [f'Frame{i:06d}.png' for i in range(len(frames))]):
        errors.append('Missing consecutive frames')
    if frames:
        encoded = subprocess.run([ffmpeg, '-y', '-framerate', '60', '-i', str(frames[0].parent / 'Frame%06d.png'),
            '-c:v', 'libx264', '-crf', '18', '-pix_fmt', 'yuv420p', str(out / 'continuous.mp4')], capture_output=True)
        (out / 'encode.log').write_bytes(encoded.stderr)
        if encoded.returncode: errors.append('Encoding failed')
    result = dict(status='FAIL' if errors else 'RECORDED', errors=errors, frames=len(frames), fps=60,
        fixed_step=not args.no_film, audio=False, manual_input=False, performance_measurement=False,
        subjective_acceptance=False, rendering=not args.headless, command=list(map(str, command)))
    (out / 'report.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result), flush=True)
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
