"""Pure authored encounter composition; preserves all non-spawn wave/stage fields."""
import copy
import json
from pathlib import Path

SPEC=json.loads((Path(__file__).parent/'Data/MonsterVariations.json').read_text(encoding='utf-8'))
P09_IDS=frozenset(i for grade in SPEC['p09_grades'] for i in grade['ids'])
CREATURE_IDS=frozenset(c['id'] for c in SPEC['creatures'])
IDS=P09_IDS|CREATURE_IDS
ROLE_IDS={i:g['base'] for g in SPEC['p09_grades'] for i in g['ids']}
ROLE_IDS.update({c['id']:c['base'] for c in SPEC['creatures']})

def compose(stages):
    result=copy.deepcopy(stages)
    assert set(map(int,SPEC['waves'])) <= {s['Id'] for s in result}
    for stage in result:
        roster=SPEC['waves'].get(str(stage['Id']))
        if roster is None: continue
        assert len(stage['Waves'])==len(roster)
        for wave, pack in zip(stage['Waves'],roster):
            old=wave['MonsterSpawnInfos']
            assert old and len({s['MonsterId'] for s in old})==len(old)
            metadata=lambda s:{k:v for k,v in s.items() if k not in ('MonsterId','SpawnCount')}
            assert all(metadata(s)==metadata(old[0]) for s in old), 'Mixed spawn timing needs explicit migration'
            assert sum(s['SpawnCount'] for s in old)==sum(n for _,n in pack), 'Wave budget changed'
            assert len({i for i,_ in pack})==len(pack) and all(n>0 for _,n in pack)
            wave['MonsterSpawnInfos']=[dict(old[0],MonsterId=i,SpawnCount=n) for i,n in pack]
    return result

def validate_roster(stages):
    assert compose(stages)==stages, 'Monster variation roster differs from source'
    assert IDS <= {s['MonsterId'] for stage in stages for w in stage['Waves'] for s in w['MonsterSpawnInfos']}
