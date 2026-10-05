"""Rendered frame-by-frame pose evidence, separate from spatial/pass acceptance."""
import argparse
import hashlib
from datetime import datetime, timezone
import json
import os
import re
import uuid
from pathlib import Path
from RunQA import ROOT, FATAL, read_text, run_process


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--label', default='motion')
    parser.add_argument('--fps', type=int, default=60)
    parser.add_argument('--p1', action='store_true')
    parser.add_argument('--combo', action='store_true', help='Hold/release the real basic combo and inspect outgoing/incoming weights')
    parser.add_argument('--capture', action='store_true', help='Also capture stills; readback stalls invalidate frame-time comparisons')
    parser.add_argument('--packaged-exe', type=Path)
    parser.add_argument('--configuration', choices=('Development','DebugGame'), default='Development')
    args = parser.parse_args()
    if args.combo and args.p1:
        parser.error('--combo and --p1 are separate probes')
    version = json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/'Epic Games'/('UE_'+version)
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_')+uuid.uuid4().hex[:6]
    out = ROOT/'Saved/QA'/(run_id+'_attack_motion_'+args.label)
    out.mkdir(parents=True)
    editor = 'UnrealEditor-Win64-DebugGame.exe' if args.configuration == 'DebugGame' else 'UnrealEditor.exe'
    executable = args.packaged_exe.resolve() if args.packaged_exe else engine/'Engine/Binaries/Win64'/editor
    if not executable.is_file():
        parser.error('Missing executable: '+str(executable))
    command = ([executable] if args.packaged_exe else [executable, ROOT/'UPlayground.uproject']) + [
               '/Game/Maps/RogueArena', '-game', '-PGHackSlashMotionTrace',
               f'-ExecCmds=t.MaxFPS {args.fps},PGHackSlashProbe', '-RenderOffscreen', '-windowed',
               '-ForceRes', '-ResX=1280', '-ResY=720', '-nosound', '-unattended', '-nop4', '-culture=en',
               '-DisablePlugins=RiderLink', '-ddc=InstalledNoZenLocalFallback',
               '-PGTestProfile=HackSlash_Motion_'+uuid.uuid4().hex[:12],
               '-UserDir='+str(out/'User'), '-abslog='+str(out/'motion.log')]
    if args.p1:
        command.append('-PGHackSlashP1Probe')
    if args.combo:
        command.append('-PGHackSlashComboProbe')
    if args.capture:
        command.append('-PGHackSlashCapture')
    print(out, flush=True)
    code, timeout = run_process(command, ROOT, out/'stdout.log', 180)
    log = read_text(out/'motion.log') if (out/'motion.log').exists() else ''
    if args.combo:
        begins = [(int(n),int(skill),float(world)) for n,skill,world in
                  re.findall(r'PGCombo Begin=(\d+) Skill=(\d+) World=([\d.]+)',log)]
        blends = [(float(a),float(b),float(p)) for a,b,p in
                  re.findall(r'PGCombo Blend In=([\d.]+) Out=([\d.]+) Position=([\d.]+)',log)]
        poses = re.findall(r'PGCombo Pose Frame=(\d+) Skill=(\d+) Clock=([\d.]+) MaxBoneAngle=([\d.]+) Bones=(\d+)',log)
        errors = []
        if code or timeout or FATAL.search(log) or 'PGHackSlashProbe PASS combo=100,101,102,100' not in log:
            errors.append(f'Combo runtime failure: exit={code}, timeout={timeout}')
        if [s for _,s,_ in begins] != [100,101,102,100] or len(blends)<6 or not poses:
            errors.append('Missing combo order, evaluated poses or crossfade samples')
        if any(a+b<.95 or not 0<a<1 or not 0<b<1 for a,b,_ in blends):
            errors.append('Invalid crossfade weights')
        intervals = [b[2]-a[2] for a,b in zip(begins,begins[1:])]
        spec = json.loads(read_text(ROOT/'Tools/Validation/HackSlashP0.json'))
        expected = [s['attack'] for s in spec['skills'] if s['id'] in (100,101,102)]
        if len(intervals)!=3 or any(abs(a-b)>.10 for a,b in zip(intervals,expected)):
            errors.append('Combo cancel cadence differs from authored timing')
        report = dict(status='FAIL' if errors else 'PASS',errors=errors,begins=begins,
                      intervals=intervals,crossfades=blends,poses=poses,
                      direct_input=False,continuous_visual_acceptance=False,
                      command=[str(c) for c in command])
        (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        print(json.dumps({k:report[k] for k in ('status','errors','intervals')}),flush=True)
        return bool(errors)
    frames = []
    pattern = r'PGMotion Skill=(\d+) Frame=(\d+) DT=([\d.]+) Clock=([\d.]+) Position=([\d.]+) Frozen=(\d+) MaxBoneAngle=([\d.]+) Bones=(\d+)'
    for match in re.finditer(pattern, log):
        skill, frame, dt, clock, position, frozen, angle, bones = match.groups()
        frames.append(dict(skill=int(skill), frame=int(frame), dt=float(dt), clock=float(clock),
                           position=float(position), frozen=bool(int(frozen)), max_bone_angle=float(angle), bones=int(bones)))
    summaries = []
    expected = [110, 113, 114] if args.p1 else [100, 101, 102, 111, 112]
    spec = json.loads(read_text(ROOT/'Tools/Validation'/('HackSlashP1.json' if args.p1 else 'HackSlashP0.json')))
    by_id = {s['id']: s for s in spec['skills']}
    errors = []
    for skill in expected:
        samples = [f for f in frames if f['skill'] == skill]
        # Zero-motion samples are evidence, not a failure: hit-stop and authored holds must be distinguished.
        advancing = [b for a, b in zip(samples, samples[1:]) if b['clock'] > a['clock']+0.0001 and not b['frozen']]
        # Use logical time so intentional hit-stop and frenzy do not masquerade as
        # an incorrectly compressed pose map. Very small rounded deltas are noisy.
        rates = [(b['position']-a['position'])/(b['clock']-a['clock'])
                 for a,b in zip(samples,samples[1:]) if b['clock']-a['clock'] >= .005]
        peak_rate = max(rates, default=0.)
        summaries.append(dict(skill=skill, frames=len(samples), advancing=len(advancing),
                              peak_pose_rate=peak_rate,
                              unchanged_while_advancing=sum(f['max_bone_angle'] < 0.001 for f in advancing),
                              max_bone_angle=max((f['max_bone_angle'] for f in samples), default=0)))
        if len(samples) < 5 or any(f['bones'] == 0 for f in samples):
            errors.append(f'Missing evaluated pose samples: {skill}')
        if 'max_pose_rate' in by_id[skill] and (not rates or peak_rate > by_id[skill]['max_pose_rate']+.02):
            errors.append(f'Active skill pose compression exceeds authored limit: {skill} rate={peak_rate}')
    if code or timeout or FATAL.search(log) or 'PGHackSlashProbe PASS' not in log:
        errors.append(f'Runtime failure: exit={code}, timeout={timeout}')
    report = dict(status='FAIL' if errors else 'RECORDED', errors=errors, command=[str(c) for c in command],
                  skills=summaries, frames=frames, direct_input=False, continuous_visual_acceptance=False,
                  capture_readback_stalls=args.capture,
                  executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest() if args.packaged_exe else None)
    (out/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(summaries), flush=True)
    print('status='+report['status'], flush=True)
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
