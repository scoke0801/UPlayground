"""Render close-ups and record hand/weapon transforms using a disposable profile."""
import argparse
import csv
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import uuid
import re
from RunPlayableCharacters import ROOT, run_process, read_text, FATAL


def contact_error(pose_path, calibration, identity):
    row = next(p for p in calibration['profiles'] if p['identity'] == identity)
    with pose_path.open(encoding='utf-8-sig') as stream:
        poses = list(csv.DictReader(stream))
    hand = next(p for p in poses if p['mesh'] == 'target' and p['bone'].casefold() == row['target_socket'].casefold() and p['space'] == 'world')
    weapon = next(p for p in poses if p['mesh'] == 'weapon' and p['bone'] == 'root' and p['space'] == 'world')
    def multiply(a, b):
        x,y,z,w = a; X,Y,Z,W = b
        return [w*X+x*W+y*Z-z*Y, w*Y-x*Z+y*W+z*X, w*Z+x*Y-y*X+z*W, w*W-x*X-y*Y-z*Z]
    def world(pose, contact):
        q = [float(pose[k]) for k in ('qx','qy','qz','qw')]
        v = [contact['translation'][i]*float(pose['s'+axis]) for i,axis in enumerate('xyz')]
        rotated = multiply(multiply(q, v+[0]), [-q[0],-q[1],-q[2],q[3]])[:3]
        return [float(pose[axis])+rotated[i] for i,axis in enumerate('xyz')], multiply(q, contact['rotation'])
    a,qa = world(hand, row['hand_contact']); b,qb = world(weapon, row['weapon_contact'])
    dot = abs(sum(x*y for x,y in zip(qa,qb))) / math.sqrt(sum(x*x for x in qa)*sum(x*x for x in qb))
    return dict(position_cm=math.dist(a,b), angle_degrees=math.degrees(2*math.acos(min(1.,dot))),
                weapon_world_scale=[float(weapon['s'+axis]) for axis in 'xyz'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--identity', default='Bokusei')
    parser.add_argument('--candidate', type=Path, help='UE exported FPGAppearanceGripProfile; applied in memory only')
    parser.add_argument('--motion', action='store_true', help='Eight GAS attacks, dodge and sheath/equip with per-frame grip checks')
    parser.add_argument('--calibration', type=Path, help='Authored contact frames; require <=1 cm / 5 degree world alignment')
    args = parser.parse_args()
    output = ROOT/'Saved/PlayableCharacters/Runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')+'_grip-preview')
    output.mkdir(parents=True)
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
    command = [engine, ROOT/'UPlayground.uproject', '/Game/Maps/RogueArena', '-game',
        '-ExecCmds=t.MaxFPS 60,PGGripPreview', '-PGGripIdentity='+args.identity,
        '-PGGripPreviewOutput='+str(output), '-PGTestProfile=Characters_Grip_'+uuid.uuid4().hex[:12],
        '-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=960',
        '-unattended', '-nosound', '-nop4', '-culture=en', '-DisablePlugins=RiderLink', '-Multiprocess',
        '-ddc=InstalledNoZenLocalFallback', '-abslog='+str(output/'engine.log')]
    if args.candidate:
        command.append('-PGGripCandidate='+str(args.candidate.resolve(strict=True)))
        (output/'candidate.txt').write_bytes(args.candidate.read_bytes())
    if args.motion: command.append('-PGGripMotion')
    (output/'command.json').write_text(json.dumps([str(c) for c in command], indent=2))
    print(output, flush=True)
    code, timeout = run_process(command, ROOT, output/'stdout.log', 180)
    log = read_text(output/'engine.log')
    ok = code == 0 and not timeout and not FATAL.search(log) and 'PGGripPreview PASS' in log
    ok = ok and (output/'pose.csv').exists() and all((output/f'idle_{i}.png').exists() for i in range(4))
    if args.motion:
        ok = ok and all(f'PGGripPreview Motion={i} PASS' in log for i in range(11))
        ok = ok and all((output/f'motion_{motion:02d}_{frame:02d}.png').exists()
                        for motion in range(11) for frame in range(6))
    report = dict(code=code, identity=args.identity, visual_acceptance=False, direct_input=False,
                  scripted_attachment_lifecycle=args.motion)
    if args.calibration:
        report['contact'] = contact_error(output/'pose.csv', json.loads(args.calibration.read_text(encoding='utf-8')), args.identity)
        ok = ok and report['contact']['position_cm'] <= 1 and report['contact']['angle_degrees'] <= 5
    samples = re.findall(r'PGGripPreview Sample Motion=(\d+) Distance=([\d.]+) Angle=([\d.]+) Fingers=([\d.]+)', log)
    if samples:
        report['samples'] = len(samples)
        report['max_relative_drift_local'] = max(float(s[1]) for s in samples)
        report['max_relative_angle_degrees'] = max(float(s[2]) for s in samples)
        report['max_finger_angle_degrees'] = max(float(s[3]) for s in samples)
    report['status'] = 'PASS' if ok else 'FAIL'
    (output/'result.json').write_text(json.dumps(report, indent=2))
    print('PASS' if ok else 'FAIL', flush=True)
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
