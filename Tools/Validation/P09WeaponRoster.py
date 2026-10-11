"""P09 weapon families and budget-preserving authored encounter substitutions."""
import copy
import json
from pathlib import Path

SPEC_PATH = Path(__file__).parent / 'Data/P09WeaponVariations.json'
SPEC = json.loads(SPEC_PATH.read_text(encoding='utf-8'))
DEST = '/Game/DataCenter/P09Weapons'
IDS = frozenset(m['id'] for m in SPEC['monsters'])
ROLE_IDS = {m['id']: m['base'] for m in SPEC['monsters']}


def compose(stages):
    result = copy.deepcopy(stages)
    for patch in SPEC['wave_replacements']:
        stage = next(s for s in result if s['Id'] == patch['stage'])
        wave = stage['Waves'][patch['wave'] - 1]['MonsterSpawnInfos']
        if any(s['MonsterId'] == patch['enemy'] for s in wave):
            assert sum(s['SpawnCount'] for s in wave if s['MonsterId'] == patch['enemy']) == 1
            continue
        source = next(s for s in wave if s['MonsterId'] == patch['source'])
        assert source['SpawnCount'] > 0
        added = dict(source, MonsterId=patch['enemy'], SpawnCount=1)
        source['SpawnCount'] -= 1
        wave[:] = [s for s in wave if s['SpawnCount'] > 0] + [added]
    return result


def active(stages):
    return any(s['MonsterId'] in IDS for stage in stages for w in stage['Waves'] for s in w['MonsterSpawnInfos'])


def validate_roster(stages):
    assert compose(stages) == stages, 'P09 weapon encounter roster differs'
    assert IDS <= {s['MonsterId'] for stage in stages for w in stage['Waves'] for s in w['MonsterSpawnInfos']}
