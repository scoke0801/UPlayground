"""Compare real spatial attacks before/after a transient weapon-anchor perturbation.

This gate authorizes no art calibration: hand-contact frames and visual acceptance
remain separate. Each case runs in a new process with a disposable save profile.
"""
import argparse
import hashlib
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import uuid

from RunPlayableCharacters import ROOT, FATAL, read_text, run_process
from HackSlashMetrics import fields, parse_metrics

IDENTITIES = ['Bokusei', 'LianLian', 'Honoka', 'Hichi', 'Siuha', 'Lili', 'Nenmir']
SKILLS = [100, 101, 102, 110, 111, 112, 113, 114]
TIME_TOLERANCE = 1 / 60 + .002  # Fixed 60 Hz observation; never auto-expanded.
SKILL_DEFINITIONS = {row['id']: row for name in ('HackSlashP0.json', 'HackSlashP1.json')
                     for row in json.loads(read_text(ROOT/'Tools/Validation'/name))['skills']}


def hash_files(root, paths):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}


def stage_project(output):
    """Freeze native modules/config while other work can continue building ROOT.

    Content stays shared and read-only for these game probes; this is explicitly
    not the independent-art-recovery acceptance required by P0.
    """
    project = output/'Project'
    project.mkdir()
    files = [ROOT/'UPlayground.uproject']
    for folder in ('Config', 'Source'):
        files.extend(p for p in (ROOT/folder).rglob('*') if p.is_file())
    manifests = [ROOT/'Binaries/Win64/UnrealEditor.modules']
    for plugin in (ROOT/'Plugins').glob('*/'):
        files.extend(plugin.glob('*.uplugin'))
        files.extend(p for p in (plugin/'Source').rglob('*') if p.is_file())
        for folder in ('Config', 'Content', 'Resources'):
            files.extend(p for p in (plugin/folder).rglob('*') if p.is_file())
        manifest = plugin/'Binaries/Win64/UnrealEditor.modules'
        if manifest.exists(): manifests.append(manifest)
    for manifest in manifests:
        files.append(manifest)
        for name in json.loads(read_text(manifest))['Modules'].values():
            if Path(name).name != name: raise ValueError('Module path must be a filename')
            files.append(manifest.parent/name)
    files.extend((ROOT/'Binaries/Win64').glob('UPlaygroundEditor.target'))
    files.extend(ROOT/'Tools/Validation'/name for name in ('Data/PlayableCharacterPolish.json','HackSlashP0.json','HackSlashP1.json'))
    files = sorted(set(files))
    before = hash_files(ROOT, files)
    for src in files:
        dst = project/src.relative_to(ROOT)
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    copied = [project/p.relative_to(ROOT) for p in files]
    if hash_files(ROOT, files) != before or hash_files(project, copied) != before:
        raise RuntimeError('Project changed while staging; keep this failed snapshot and retry after the build')
    if os.name == 'nt':
        subprocess.run(['cmd', '/c', 'mklink', '/J', str(project/'Content'), str(ROOT/'Content')],
                       check=True, capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
    else:
        (project/'Content').symlink_to(ROOT/'Content', target_is_directory=True)
    (output/'snapshot.json').write_text(json.dumps(dict(project=str(project), hashes=before,
        shared_content=str(ROOT/'Content'), independent_art_recovery=False), indent=2), encoding='utf-8')
    return project, copied, before


def rows(log, marker):
    return [fields(line.split(marker, 1)[1]) for line in log.splitlines()
            if 'LogTemp:' in line and marker in line]


def inspect(log, identity, stress):
    errors = []
    probes = rows(log, 'PGHackSlashProbe ')
    probes = [p for p in probes if 'Skill' in p]
    metrics, more = parse_metrics(log, dict(metrics='1', world='0', variant='p1'),
        [dict(target='0', loss=p['Damage']) for p in probes],
        [dict(skill=str(skill)) for skill in SKILLS])
    errors.extend(more)
    if len(probes) != 16:
        errors.append('Expected 16 independent target HP checks')
    setup = rows(log, 'PGGrip Setup ')
    if len(setup) != 1 or setup[0].get('Identity') != identity or setup[0].get('Stress') != str(int(stress)):
        errors.append('Missing/mismatched attachment stimulus')
    elif not math.isfinite(float(setup[0]['Moved'])) or (float(setup[0]['Moved']) < 5 if stress else float(setup[0]['Moved']) != 0):
        errors.append('Actual weapon did not receive the requested stimulus')
    samples = rows(log, 'PGGrip Sample ')
    for skill in SKILLS:
        if sum(int(s['Skill']) == skill for s in samples) < 5:
            errors.append(f'Missing evaluated attachment samples: {skill}')
    if any(not math.isfinite(float(v)) for s in samples for v in s.values()):
        errors.append('Nonfinite attachment sample')
    presentation = rows(log, 'PGSkill Presentation ')
    collisions = rows(log, 'PGGrip Collision ')
    collisions = [c for c in collisions if int(c['Skill']) in SKILLS]
    notifies = rows(log, 'PGGrip Notify ')
    vfx = rows(log, 'PGGrip VFX ')
    if not notifies:
        errors.append('Missing dispatched combat notify observations')
    for skill in SKILLS:
        definition = SKILL_DEFINITIONS[skill]
        expected = len(definition['hits'])
        if sum(int(p['Skill']) == skill for p in presentation) != expected:
            errors.append(f'Missing/duplicate presentation phase: {skill}')
        if not any(int(c['Skill']) == skill for c in collisions):
            errors.append(f'Missing collision request observations: {skill}')
        disc = definition.get('shape') == 'DISC' or definition['angle'] == 360
        vfx_counts = {expected*(2 if disc else 1)}
        # ProjectileSwingVFX is optional: one request for the projectile itself,
        # plus one per phase when its separate caster swing is configured.
        if definition.get('shape') == 'PROJECTILE': vfx_counts.add(expected*2)
        if sum(int(v['Skill']) == skill for v in vfx) not in vfx_counts:
            errors.append(f'Missing/duplicate VFX spawn request: {skill}')
    if any(n['Source'] != '1' for n in notifies):
        errors.append('Visible retarget mesh dispatched a combat notify')
    if any(v['Available'] != '1' for v in vfx):
        errors.append('VFX asset missing')
    if any(c['Effective'] != '0' for c in collisions):
        errors.append('Legacy collision enabled in a profile cast')
    if FATAL.search(log) or 'PGHackSlashProbe PASS skills=8 grip=1' not in log:
        errors.append('Runtime did not complete the eight-attack gate')
    return dict(errors=errors, casts=metrics['casts'], targets=probes,
                presentation=presentation, collisions=collisions, notifies=notifies, vfx=vfx,
                attachment_samples=samples, setup=setup)


def compare(before, after):
    errors = list(before['errors']) + list(after['errors'])
    def near(a, b, tolerance, label):
        if not math.isfinite(float(a)) or not math.isfinite(float(b)) or abs(float(a)-float(b)) > tolerance:
            errors.append(label)
    if len(before['casts']) != 8 or len(after['casts']) != 8:
        errors.append('Expected eight complete casts per variant')
        return errors
    for skill in SKILLS:
        a = [s for s in before['attachment_samples'] if int(s['Skill']) == skill]
        b = [s for s in after['attachment_samples'] if int(s['Skill']) == skill]
        if not a or not b or all(abs(float(a[0][k])-float(b[0][k])) < .001 for k in ('X','Y','Z','QX','QY','QZ','QW')):
            errors.append(f'{skill}: attachment stimulus absent during attack')
    for a, b in zip(before['casts'], after['casts']):
        label = str(a['skill'])
        for key in ('skill', 'cancelled', 'direct_damage', 'counters'):
            if a.get(key) != b.get(key): errors.append(f'{label}: cast {key} changed')
        if a.get('end') is None or b.get('end') is None:
            errors.append(f'{label}: incomplete observation')
            continue
        near(a['end']-a['started'], b['end']-b['started'], TIME_TOLERANCE, label+': cast duration changed')
        near(a['displacement_cm'], b['displacement_cm'], .01, label+': movement changed')
        if len(a['hits']) != len(b['hits']): errors.append(label+': hit count changed')
        for x, y in zip(a['hits'], b['hits']):
            for key in ('phase', 'target', 'damage', 'behind'):
                if x[key] != y[key]: errors.append(f'{label}: hit {key} changed')
            near(x['time']-a['started'], y['time']-b['started'], TIME_TOLERANCE, label+': hit time changed')
    for section in ('targets', 'presentation', 'collisions', 'notifies', 'vfx'):
        a, b = before[section], after[section]
        if len(a) != len(b): errors.append(section+': event count changed')
        for x, y in zip(a, b):
            if x.keys() != y.keys():
                errors.append(section+': fields changed')
                continue
            for key in x:
                near(x[key], y[key], TIME_TOLERANCE if key == 'Time' else .001, f'{section}: {key} changed')
    return errors


def review(directory):
    """Re-evaluate preserved logs without altering the original run report.

    Process failures and changed-binary failures are carried forward, never
    inferred away from a PASS marker emitted before process shutdown.
    """
    original = json.loads(read_text(directory/'report.json'))
    result = dict(status='FAIL', source_run=str(directory), comparisons=[],
                  original_report=original, replayed=False)
    for case in original['comparisons']:
        identity = case['identity']
        variants = []
        for stress in (False, True):
            folder = directory/(identity+('_stress' if stress else '_baseline'))
            previous = json.loads(read_text(folder/'observations.json'))
            current = inspect(read_text(folder/'engine.log'), identity, stress)
            current['errors'].extend(e for e in previous['errors']
                                     if e.startswith(('Process exit=', 'Frozen module was not loaded:')))
            variants.append(current)
        errors = compare(*variants)
        result['comparisons'].append(dict(identity=identity, status='FAIL' if errors else 'PASS', errors=errors))
    expected = original.get('requested_identities', IDENTITIES)
    complete = [c['identity'] for c in result['comparisons']] == expected
    result['status'] = 'FAIL' if original.get('exception') or not complete or any(c['errors'] for c in result['comparisons']) else 'PASS'
    result['complete'] = complete
    result['validator_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    result['skill_definitions'] = SKILL_DEFINITIONS
    (directory/'review.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(result['status'], [(c['identity'],c['errors'][:3]) for c in result['comparisons']], flush=True)
    return 0 if result['status'] == 'PASS' else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--identity', choices=IDENTITIES, action='append', help='Default: all seven')
    parser.add_argument('--calibrate', type=Path, help='Authored contact-frame JSON; write a candidate without saving assets')
    parser.add_argument('--calibration-self-test', action='store_true')
    parser.add_argument('--review-run', type=Path, help='Re-evaluate complete archived observations; preserves original report')
    args = parser.parse_args(argv)
    if args.review_run:
        if args.identity or args.calibrate or args.calibration_self_test: parser.error('--review-run cannot be combined with execution options')
        return review(args.review_run.resolve(strict=True))
    out = ROOT/'Saved/PlayableCharacters/Runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')+'_grip')
    out.mkdir(parents=True)
    if args.calibrate or args.calibration_self_test:
        if args.identity or (args.calibrate and args.calibration_self_test):
            parser.error('Select one of combat comparison, calibration or calibration self-test')
        from RunPlayableCharacterPolish import editor
        previous = os.environ.pop('PG_GRIP_CALIBRATION', None)
        try:
            if args.calibrate: os.environ['PG_GRIP_CALIBRATION'] = str(args.calibrate.resolve(strict=True))
            editor('CalibratePlayableCharacterGrip.py', out)
        finally:
            os.environ.pop('PG_GRIP_CALIBRATION', None)
            if previous is not None: os.environ['PG_GRIP_CALIBRATION'] = previous
        print(out, flush=True)
        return 0
    version = json.loads(read_text(ROOT/'UPlayground.uproject'))['EngineAssociation']
    exe = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))/f'Epic Games/UE_{version}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    result = dict(status='FAIL', comparisons=[], fixed_fps=60, time_tolerance_seconds=TIME_TOLERANCE,
                  art_tuning=False, visual_acceptance=False, direct_input=False,
                  legacy_spatial_acceptance=False, rendered=False)
    result['requested_identities'] = args.identity or IDENTITIES
    print(out, flush=True)
    try:
        project, copied, hashes = stage_project(out)
        result['input_hashes'] = hashes
        result['runtime_project'] = str(project/'UPlayground.uproject')
        result['shared_content'] = str(ROOT/'Content')
        for identity in args.identity or IDENTITIES:
            variants = []
            for stress in (False, True):
                folder = out/(identity+('_stress' if stress else '_baseline'))
                folder.mkdir()
                cmd = [exe, project/'UPlayground.uproject', '/Game/Maps/RogueArena', '-game',
                       '-ExecCmds=t.MaxFPS 60,pg.Skill.DebugCast 1,pg.Skill.Observe 1,PGHackSlashProbe',
                       '-PGGripIdentity='+identity, '-PGGripTrace', '-UseFixedTimeStep', '-FPS=60',
                       '-PGTestProfile=HackSlash_Grip_'+uuid.uuid4().hex[:12], '-UserDir='+str(folder/'User'),
                       '-nullrhi', '-unattended', '-nosound', '-nop4', '-culture=en', '-DisablePlugins=RiderLink',
                       '-ddc=InstalledNoZenLocalFallback', '-abslog='+str(folder/'engine.log')]
                if stress: cmd.append('-PGGripStress')
                (folder/'command.json').write_text(json.dumps([str(c) for c in cmd], indent=2), encoding='utf-8')
                code, timeout = run_process(cmd, ROOT, folder/'stdout.log', 240)
                log = read_text(folder/'engine.log') if (folder/'engine.log').exists() else ''
                observation = inspect(log, identity, stress)
                if code or timeout: observation['errors'].append(f'Process exit={code}, timeout={timeout}')
                observation['process'] = dict(code=code, timeout=timeout)
                for module in ('PGActor', 'UPlayground'):
                    expected = (project/f'Binaries/Win64/UnrealEditor-{module}.dll').as_posix().lower()
                    if expected not in log.replace('\\', '/').lower():
                        observation['errors'].append('Frozen module was not loaded: '+module)
                (folder/'observations.json').write_text(json.dumps(observation, indent=2), encoding='utf-8')
                variants.append(observation)
            errors = compare(*variants)
            result['comparisons'].append(dict(identity=identity, status='FAIL' if errors else 'PASS', errors=errors))
            print(identity, result['comparisons'][-1]['status'], errors[:5], flush=True)
        result['status'] = 'FAIL' if any(r['errors'] for r in result['comparisons']) else 'PASS'
        if hash_files(project, copied) != hashes:
            result.update(status='FAIL', exception='Frozen project changed during comparison')
    except Exception as exc:
        result['exception'] = repr(exc)
    finally:
        (out/'report.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
