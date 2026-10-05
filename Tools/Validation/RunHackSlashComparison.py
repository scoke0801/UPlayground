"""Launch isolated P0 trials or prepare the 4 x 5 x 2 manual comparison matrix.

Smoke validates setup and telemetry only. No automated input, healing or contact injection.
Run each manual attempt in a fresh process; failures and aborted trials are retained.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys
import uuid
from RunQA import ROOT, FATAL, read_text, run_process
from HackSlashMetrics import parse_metrics, number

SCENARIOS = {'P0-M10': 12, 'P0-M15': 18, 'P0-RING': 14, 'P0-E1': 1}
SEEDS = (173001, 173002, 173003, 173004, 173005)
DEFAULT_BASELINE = ROOT / 'Saved/Backups/HackSlashP0/20261004T060946315194Z/skills.json'


def summarize_trials(directory):
    """Keep every attempt (including failures); smoke never fills a manual matrix cell."""
    cells = {(s, seed, v): [] for s in SCENARIOS for seed in SEEDS for v in ('baseline', 'p0')}
    smoke_reports, strengthened_reports, invalid_reports = [], [], []
    for path in sorted(directory.rglob('report.json')):
        try:
            report = json.loads(read_text(path))
            if not isinstance(report, dict):
                raise ValueError('Report must be a JSON object')
        except (ValueError, OSError) as exc:
            invalid_reports.append(dict(report=str(path), error=str(exc)))
            continue
        key = (report.get('scenario'), report.get('seed'), report.get('variant'))
        if key not in cells:
            continue
        if report.get('status') not in ('RECORDED','FAIL'):
            invalid_reports.append(dict(report=str(path), error='Missing or invalid trial status'))
            continue
        if report.get('smoke'):
            smoke_reports.append(str(path))
            continue
        if report.get('build', 'none') != 'none':
            strengthened_reports.append(str(path))
            continue
        cells[key].append(dict(report=str(path), status=report['status'], result=report.get('result', {}),
                               errors=report.get('errors', []), video=report.get('video'),
                               direct_input_verified=report.get('direct_input_verified', False)))
    return dict(trials=[dict(scenario=s, seed=seed, variant=v, attempts=attempts,
                            status='PENDING' if not attempts else 'NEEDS_REVIEW')
                        for (s, seed, v), attempts in cells.items()], excluded_smoke=smoke_reports,
                strengthened_reports=strengthened_reports, invalid_reports=invalid_reports, p0_acceptance_complete=False)


def baseline_evidence(path):
    raw = path.read_bytes()
    rows = json.loads(raw.decode('utf-8-sig'))
    selected = {r['SkillID']: r for r in rows if r['SkillID'] in (100, 101, 102, 111, 112)}
    if len(selected) != 5 or sum(r['SkillID'] in selected for r in rows) != 5 or any(r['PlayerProfile'] not in ('None', '', None) for r in selected.values()):
        raise ValueError('Baseline must contain all five pre-P0 rows with empty PlayerProfile')
    return {'path': str(path.resolve()), 'sha256': hashlib.sha256(raw).hexdigest(), 'skills': selected}


def parse_trial(log, scenario, seed, variant, smoke, build='none'):
    try:
        return _parse_trial(log, scenario, seed, variant, smoke, build)
    except (ValueError, KeyError, TypeError, OverflowError) as exc:
        return dict(status='FAIL', errors=[f'Malformed telemetry: {exc}'], result={},
                    direct_input_verified=False, p0_acceptance_complete=False)


def _parse_trial(log, scenario, seed, variant, smoke, build):
    """Fail closed on missing/duplicate boundaries; gameplay loss is a valid recorded result."""
    lines = [line.split('PGSkillTrial ', 1)[1] for line in log.splitlines() if 'LogTemp:' in line and 'PGSkillTrial ' in line]
    begins = [line for line in lines if line.startswith('BEGIN ')]
    ends = [line for line in lines if line.startswith('END ')]
    errors = []
    if len(begins) != 1 or len(ends) != 1:
        errors.append('Expected exactly one BEGIN and END')
    elif lines.index(begins[0]) >= lines.index(ends[0]) or any(
            (line.startswith(('USE ', 'HEALTH ')) and not lines.index(begins[0]) < i < lines.index(ends[0]))
            for i,line in enumerate(lines)):
        errors.append('Events outside trial boundaries')
    def fields(line):
        return dict(re.findall(r'(\w+)=([^\s]+)', line))
    begin, end = fields(begins[0]) if begins else {}, fields(ends[0]) if ends else {}
    if begin.get('build', 'none') != build:
        errors.append('Wrong strengthened build')
    for key, expected in dict(scenario=scenario, seed=str(seed), variant=variant,
                              enemies=str(SCENARIOS[scenario]), smoke=str(int(smoke)),
                              attack='100.0', hp='1000.0', combat_assistance='0', contact_injected='0').items():
        if begin.get(key) != expected:
            errors.append(f'BEGIN {key}: {begin.get(key)!r} != {expected!r}')
    outcomes = ('smoke_complete',) if smoke else ('clear', 'death', 'timeout')
    if end.get('outcome') not in outcomes or end.get('smoke') != str(int(smoke)):
        errors.append('Missing or unexpected outcome')
    spawns = [fields(line) for line in lines if line.startswith('SPAWN ')]
    if len(spawns) != SCENARIOS[scenario] or [int(s['index']) for s in spawns] != list(range(SCENARIOS[scenario])):
        errors.append('Incomplete spawn roster')
    for spawn in spawns:
        for key in ('x','y','z','hp'):
            number(spawn,key)
    health = [fields(line) for line in lines if line.startswith('HEALTH ')]
    uses = [fields(line) for line in lines if line.startswith('USE ')]
    rows = [fields(line) for line in lines if line.startswith('ROW ')]
    if ([r.get('skill') for r in rows] != ['100', '101', '102', '111', '112']
            or any(r.get('profile') != ('1' if variant == 'p0' else '0') for r in rows)):
        errors.append('Wrong runtime skill rows/profile variant')
    if ends:
        # NaN comparisons otherwise silently pass reconciliation.
        for row, keys in [(end, ('seconds','damage','taken'))] + [(h, ('time','loss','hp')) for h in health]:
            for key in keys:
                if number(row, key) < 0:
                    errors.append('Negative telemetry: ' + key)
        hp = {-1: 1000., **{int(s['index']): number(s, 'hp') for s in spawns}}
        enemy_ids = {-1: '0', **{int(s['index']): s['enemy'] for s in spawns}}
        last_time = -1.
        for event in health:
            target, time, loss, remaining = int(event['target']), number(event,'time'), number(event,'loss'), number(event,'hp')
            if target not in hp or enemy_ids.get(target) != event['enemy'] or loss <= 0 or abs(hp[target]-loss-remaining) > .003 or time < last_time or time > number(end,'seconds')+.002:
                errors.append('Invalid health sequence/target/time')
                break
            hp[target], last_time = remaining, time
        last_use = -1.
        for use in uses:
            time = number(use,'time')
            if time < 0 or time < last_use or time > number(end,'seconds')+.002 or int(use['skill']) not in (100,101,102,111,112,10000):
                errors.append('Invalid committed skill/time')
            last_use = time
        damage = sum(float(h['loss']) for h in health if int(h['target']) >= 0)
        taken = sum(float(h['loss']) for h in health if int(h['target']) < 0)
        kills = len({h['target'] for h in health if int(h['target']) >= 0 and float(h['hp']) <= 0})
        tolerance = max(.02, .001 * (len(health) + 1))  # native log rounds each event to 3 decimals
        if abs(damage - float(end.get('damage', -1))) > tolerance or abs(taken - float(end.get('taken', -1))) > tolerance:
            errors.append('Health telemetry does not reconcile with summary')
        if kills != int(end.get('kills', -1)) or len(uses) != int(end.get('uses', -1)):
            errors.append('Kill/use telemetry does not reconcile with summary')
        if end.get('outcome') == 'clear' and kills != SCENARIOS[scenario]:
            errors.append('Clear without every target dying')
        if end.get('outcome') == 'death' and not any(int(h['target']) < 0 and float(h['hp']) <= 0 for h in health):
            errors.append('Death without player health reaching zero')
        if end.get('outcome') in ('timeout', 'smoke_complete'):
            limit = 3 if smoke else 30 if scenario == 'P0-E1' else 60
            if float(end.get('seconds', 0)) < limit:
                errors.append('Trial ended before its observation limit')
    first_archer = next((float(h['time']) for h in health if h['enemy'] == '15102'), None)
    metrics, metric_errors = parse_metrics(log, begin, health, uses)
    errors.extend(metric_errors)
    if metrics.get('status') == 'RECORDED' and any(c['end'] > number(end,'seconds')+.002 for c in metrics['casts']):
        errors.append('Cast observation extends beyond trial end')
    return dict(status='FAIL' if errors else 'RECORDED', errors=errors, begin=begin, result=end,
                cast_metrics=metrics, build=build,
                spawns=spawns, skill_rows=rows, health_events=health, skill_uses=uses, first_archer_damage_seconds=first_archer,
                direct_input_verified=False, video=None, p0_acceptance_complete=False)


def run_trial(args, scenario, seed, variant, parent):
    trial_id = f'{scenario}_{seed}_{variant}_{uuid.uuid4().hex[:8]}'
    out = parent / trial_id
    out.mkdir()
    version = json.loads(read_text(ROOT / 'UPlayground.uproject'))['EngineAssociation']
    engine = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games' / f'UE_{version}'
    baseline = baseline_evidence(args.baseline)
    command = [engine / 'Engine/Binaries/Win64/UnrealEditor.exe', ROOT / 'UPlayground.uproject',
               '/Game/Maps/RogueArena', '-game', '-windowed', '-ForceRes', '-ResX=1920', '-ResY=1080',
               '-nop4', '-nosplash', '-culture=en', '-UTF8Output', '-DisablePlugins=RiderLink',
               '-ddc=InstalledNoZenLocalFallback', f'-PGTestProfile=HackSlash_{uuid.uuid4().hex[:12]}_{scenario}',
               f'-PGRunSeed={seed}', f'-PGSkillBaseline={args.baseline.resolve()}', f'-PGSkillTrialBuild={args.build}',
               f'-ExecCmds=t.MaxFPS 60,pg.Skill.Observe 1,PGSkillScenario {scenario} {seed} {variant}',
               f'-abslog={out / "trial.log"}', '-PGSkillTrialExit']
    if args.packaged_exe:
        command = [args.packaged_exe.resolve()] + command[2:]
        command += [f'-UserDir={out / "User"}']
    if args.smoke:
        command += ['-unattended', '-nosound', '-PGSkillScenarioSmoke']
        command += (['-RenderOffscreen', f'-PGSkillTrialScreenshot={out / "scene.png"}']
                    if args.render_smoke else ['-nullrhi'])
    manifest = dict(scenario=scenario, seed=seed, variant=variant, smoke=args.smoke, build=args.build,
                    packaged_executable=str(args.packaged_exe.resolve()) if args.packaged_exe else None,
                    executable_sha256=hashlib.sha256(args.packaged_exe.read_bytes()).hexdigest() if args.packaged_exe else None,
                    baseline=baseline, command=[str(c) for c in command], p0_acceptance_complete=False,
                    p0_spec_sha256=hashlib.sha256((ROOT / 'Tools/Validation/HackSlashP0.json').read_bytes()).hexdigest())
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding='utf-8')
    code, timed_out = run_process(command, ROOT, out / 'stdout.log', 180)
    log = read_text(out / 'trial.log') if (out / 'trial.log').exists() else ''
    try:
        report = parse_trial(log, scenario, seed, variant, args.smoke, args.build)
    except (ValueError, KeyError, TypeError) as exc:
        report = dict(status='FAIL', errors=[f'Malformed telemetry: {exc}'], result={}, p0_acceptance_complete=False)
    if code or timed_out or FATAL.search(log):
        report['errors'].append(f'Process failure: exit={code} timeout={timed_out}')
        report['status'] = 'FAIL'
    if args.render_smoke:
        png = (out / 'scene.png').read_bytes() if (out / 'scene.png').exists() else b''
        resolution = list(struct.unpack('>II', png[16:24])) if len(png) >= 24 and png[:8] == b'\x89PNG\r\n\x1a\n' else None
        report['screenshot_resolution'] = resolution
        if resolution != [1920, 1080] or len(png) < 1024:
            report['errors'].append(f'Missing or wrong-resolution screenshot: {resolution}')
            report['status'] = 'FAIL'
    report.update(scenario=scenario, seed=seed, variant=variant, smoke=args.smoke, build=args.build,
                  limits=['Seed fixes initial placement, not the full AI/random combat trajectory.',
                          'Direct hits have cast attribution; delayed proc damage is only in total health loss.',
                          'Motion/FX, physical input, safe escape and packaged performance need separate evidence.'])
    (out / 'report.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    print(f'{trial_id}: {report["status"]} {report["result"].get("outcome")} {report["errors"]}', flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare', action='store_true', help='Write 40 pending manual trials; launch no game')
    parser.add_argument('--summarize', type=Path, metavar='DIRECTORY', help='Collect all attempts into 40 cells; exclude smoke')
    parser.add_argument('--smoke', action='store_true', help='3-second unattended setup check, not balance testing')
    parser.add_argument('--render-smoke', action='store_true', help='Offscreen 1080p screenshot instead of NullRHI; requires --smoke')
    parser.add_argument('--all', action='store_true', help='Smoke all 4 scenarios in both variants')
    parser.add_argument('--scenario', choices=SCENARIOS, default='P0-M10')
    parser.add_argument('--seed', type=int, choices=SEEDS, default=SEEDS[0])
    parser.add_argument('--variant', choices=('baseline', 'p0'), default='p0')
    parser.add_argument('--baseline', type=Path, default=DEFAULT_BASELINE)
    parser.add_argument('--build', choices=('none','bleed','shock','frenzy'), default='none', help='Isolated QA perk preset; excluded from base 40 cells')
    parser.add_argument('--packaged-exe', type=Path, help='Use the freshly staged Development game binary instead of UnrealEditor')
    args = parser.parse_args()
    if args.packaged_exe and (not args.packaged_exe.is_file() or args.packaged_exe.suffix.lower() != '.exe'):
        parser.error('--packaged-exe must name an existing Development game executable')
    if args.summarize:
        if not args.summarize.is_dir():
            parser.error('--summarize directory does not exist')
        destination = args.summarize / 'comparison-summary.json'
        destination.write_text(json.dumps(summarize_trials(args.summarize), indent=2, ensure_ascii=False), encoding='utf-8')
        print(destination)
        return 0
    if args.all and not args.smoke:
        parser.error('--all is only for smoke; run manual trials individually')
    if args.render_smoke and not args.smoke:
        parser.error('--render-smoke requires --smoke')
    baseline = baseline_evidence(args.baseline)
    out = ROOT / 'Saved/QA' / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ_') + uuid.uuid4().hex[:6] + '_hack_slash_comparison')
    out.mkdir(parents=True)
    print(out, flush=True)
    if args.prepare:
        trials = [dict(scenario=s, seed=seed, variant=v, status='PENDING', video=None,
                       command=[sys.executable, str(Path(__file__).resolve()), '--scenario', s, '--seed', str(seed), '--variant', v,
                                '--baseline', str(args.baseline.resolve()), '--build', args.build], build=args.build)
                  for s in SCENARIOS for seed in SEEDS for v in ('baseline', 'p0')]
        if args.packaged_exe:
            for trial in trials:
                trial['command'] += ['--packaged-exe', str(args.packaged_exe.resolve())]
        (out / 'matrix.json').write_text(json.dumps(dict(baseline=baseline, trials=trials, p0_acceptance_complete=False),
                                                  indent=2, ensure_ascii=False), encoding='utf-8')
        return 0
    cases = [(s, args.seed, v) for s in SCENARIOS for v in ('baseline', 'p0')] if args.all else [(args.scenario, args.seed, args.variant)]
    reports = []
    for case in cases:
        reports.append(run_trial(args, *case, out))
        if reports[-1]['errors']:
            break
    pair_errors = []
    if args.all and len(reports) == 8:
        for before, after in zip(reports[::2], reports[1::2]):
            if before['spawns'] != after['spawns']:
                pair_errors.append(before['scenario'] + ': baseline/P0 initial spawn roster differs')
    result = dict(status='FAIL' if any(r['errors'] for r in reports) else 'RECORDED', smoke=args.smoke,
                  trials=reports, pair_errors=pair_errors, p0_acceptance_complete=False)
    if pair_errors or len(reports) != len(cases):
        result['status'] = 'FAIL'
    (out / 'report.json').write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
    return int(result['status'] == 'FAIL')


if __name__ == '__main__':
    raise SystemExit(main())
