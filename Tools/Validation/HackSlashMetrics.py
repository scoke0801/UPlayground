"""Read optional cast observations. Fresh presses, held repeats and procs stay distinct."""
import math
import re
import statistics


def fields(line):
    return dict(re.findall(r'(\w+)=([^\s]+)', line))


def number(row, key):
    value = float(row[key])
    if not math.isfinite(value):
        raise ValueError(f'Non-finite {key}')
    return value


def parse_metrics(log, begin, health, uses, profile_phase_counts=None):
    if begin.get('metrics') != '1':
        return dict(status='UNAVAILABLE', reason='Legacy log without cast observations'), []
    errors, casts = [], {}
    allowed_skills = (100,101,102,110,111,112,113,114) if begin.get('variant') == 'p1' else (100,101,102,111,112)
    try:
        start = number(begin, 'world')
        for line in log.splitlines():
            if 'LogTemp:' not in line or 'PGSkillMetric ' not in line:
                continue
            text = line.split('PGSkillMetric ', 1)[1]
            kind = text.split()[0]
            row = fields(text)
            cast_id = row['cast']
            time = number(row, 'world') - start
            if time < -.001:
                raise ValueError('Cast event precedes trial')
            if kind == 'BEGIN':
                if cast_id in casts:
                    raise ValueError('Duplicate cast identity')
                skill = int(row['skill'])
                if skill not in allowed_skills:
                    raise ValueError('Unexpected observed skill')
                if row['profile'] != ('1' if begin['variant'] in ('p0','p1') else '0'):
                    raise ValueError('Cast profile variant differs from trial')
                input_at = number(row, 'input')
                if input_at != -1 and (input_at < start or input_at > time + start + .001):
                    raise ValueError('Input timestamp outside cast/trial')
                casts[cast_id] = dict(cast_id=cast_id, skill=skill, started=time, profile=row['profile'] == '1',
                                      input_seconds=input_at-start if input_at >= 0 else None,
                                      hits=[], end=None)
                continue
            cast = casts[cast_id]
            if int(row['skill']) != cast['skill'] or time < cast['started']:
                raise ValueError('Cast event identity/time mismatch')
            if cast['end'] is not None:
                raise ValueError('Event after cast end')
            if kind == 'HIT':
                damage, phase = number(row, 'damage'), int(row['phase'])
                if damage <= 0 or row['behind'] not in ('0', '1'):
                    raise ValueError('Invalid direct hit')
                if cast['profile']:
                    allowed_phases = (range(profile_phase_counts[cast['skill']]) if profile_phase_counts is not None else
                                      (0,1,2) if cast['skill']==113 else (0, 1) if cast['skill'] in (111, 112) else (0,))
                    if phase not in allowed_phases:
                        raise ValueError('Unexpected profile hit phase')
                    if any(h['phase'] == phase and h['target'] == row['target'] for h in cast['hits']):
                        raise ValueError('Duplicate profile target/phase')
                cast['hits'].append(dict(time=time, target=row['target'], phase=phase,
                                         damage=damage, behind=row['behind'] == '1'))
            elif kind == 'END':
                displacement = number(row, 'displacement')
                if displacement < 0 or row['cancelled'] not in ('0', '1'):
                    raise ValueError('Invalid cast completion')
                if int(row['hits']) != len(cast['hits']) or abs(number(row, 'damage') - sum(h['damage'] for h in cast['hits'])) > .002*(len(cast['hits'])+1):
                    raise ValueError('Direct hit summary mismatch')
                counters = {k: int(row[k]) for k in ('queries', 'frenzy', 'shock', 'refund')}
                if cast['profile'] and not (counters['queries'] >= 0 and 0 <= counters['frenzy'] <= 3 and
                                           counters['shock'] in (0, 1) and counters['refund'] in (0, 1)):
                    raise ValueError('Invalid profile query/proc counts')
                cast.update(end=time, cancelled=row['cancelled'] == '1', displacement_cm=displacement,
                            counters=counters, direct_damage=number(row, 'damage'))
            else:
                raise ValueError('Unknown cast event')
        if any(c['end'] is None for c in casts.values()):
            raise ValueError('Incomplete cast observation')
        expected = [int(u['skill']) for u in uses if int(u['skill']) in allowed_skills]
        if [c['skill'] for c in casts.values()] != expected:
            raise ValueError('Committed uses and observed casts differ')
        if sum(c['direct_damage'] for c in casts.values()) > sum(number(h, 'loss') for h in health if int(h['target']) >= 0) + .01*(len(health)+1):
            raise ValueError('Direct damage exceeds total effective health loss')
        for cast in casts.values():
            taken = [h for h in health if int(h['target']) < 0 and h.get('active_cast') == cast['cast_id']]
            if any(not cast['started']-.002 <= number(h, 'time') <= cast['end']+.002 for h in taken):
                raise ValueError('Incoming damage outside active cast')
            cast['damage_taken'] = sum(number(h, 'loss') for h in taken)
            cast['incoming_hits'] = len(taken)
            cast['first_input_to_hit_seconds'] = (cast['hits'][0]['time'] - cast['input_seconds']
                if cast['hits'] and cast['input_seconds'] is not None else None)
            cast['both_phases_hit'] = {h['phase'] for h in cast['hits']} >= {0, 1}
    except (KeyError, ValueError, TypeError, IndexError) as exc:
        errors.append(f'Cast telemetry: {exc}')
    skills = []
    if not errors:
        for skill in sorted({c['skill'] for c in casts.values()}):
            items = [c for c in casts.values() if c['skill'] == skill]
            latencies = [c['first_input_to_hit_seconds'] for c in items if c['first_input_to_hit_seconds'] is not None]
            skills.append(dict(skill=skill, casts=len(items), hit_casts=sum(bool(c['hits']) for c in items),
                               effective_hit_ratio=sum(bool(c['hits']) for c in items)/len(items),
                               direct_damage=sum(c['direct_damage'] for c in items),
                               first_press_latency_samples=latencies,
                               first_press_latency_median=statistics.median(latencies) if latencies else None,
                               both_phases_hit_ratio=sum(c['both_phases_hit'] for c in items)/len(items) if skill in (111,112) and begin['variant']=='p0' else None,
                               displacement_cm=[c['displacement_cm'] for c in items],
                               damage_taken=sum(c['damage_taken'] for c in items),
                               incoming_hits=sum(c['incoming_hits'] for c in items),
                               cancellations=sum(c['cancelled'] for c in items)))
    return dict(status='FAIL' if errors else 'RECORDED', casts=list(casts.values()), skills=skills,
                limits=['Input time begins at the accepted ASC press callback, not hardware polling.',
                        'Held repeats and direct Ability activations have no fresh-press sample; a programmatic input callback may have one.',
                        'Displacement is horizontal start-to-end distance, not curved path length.',
                        'Direct damage excludes secondary proc damage; delayed procs are not assigned to the active cast.',
                        'Frenzy/shock/refund are shared cast counters, not independent proc event audits.',
                        'Input provenance, motion and safe escape still require video review.']), errors


def check_spatial_observations(log, p1=False):
    """Cross-check observations against the probe's independent target-HP measurements.

    The probe calls the input callback programmatically. This never verifies physical input.
    """
    probes = [fields(line) for line in log.splitlines() if 'LogTemp:' in line and 'PGHackSlashProbe Skill=' in line]
    expected = [110,113,114] if p1 else [100,101,102,111,112]
    health = [dict(target='0',loss=row['Damage']) for row in probes]
    result, errors = parse_metrics(log, dict(metrics='1', world='0', variant='p1' if p1 else 'p0'), health,
                                  [dict(skill=str(skill)) for skill in expected])
    if len(probes) != len(expected)*2:
        errors.append('Expected two independent target HP observations per skill')
    moves = [600,60,0] if p1 else [20,25,60,450,0]
    damages = [440,285,320] if p1 else [90,100,150,180,400]
    for cast, skill, displacement, damage in zip(result.get('casts', []), expected, moves, damages):
        if cast.get('skill') != skill or abs(cast.get('displacement_cm', -999)-displacement) > 1.:
            errors.append(f'Spatial observation displacement mismatch: {skill}')
        if abs(cast.get('direct_damage', -999)-damage) > .01 or cast.get('cancelled') is not False:
            errors.append(f'Spatial observation damage/end mismatch: {skill}')
        if cast.get('counters',{}).get('queries',0) <= 0:
            errors.append(f'Missing spatial queries: {skill}')
    result.update(status='FAIL' if errors else 'RECORDED', errors=errors, direct_input_verified=False,
                  programmatic_input=True, p0_acceptance_complete=False)
    return result
