"""P09 substitutions within the existing chaser budget (one-based stage/wave)."""
from collections import Counter
import copy

P09_IDS = frozenset(range(15201, 15205))
CHASERS = P09_IDS | {15101}
ROSTER = {
    1: ([15201,15202], [15202,15201], [15201]),
    2: ([15202,15201], [15202], []),
    3: ([15203,15204], [15201,15202], []),
    4: ([15203,15204,15201], [15204], []),
    5: ([15203,15204,15202], [15203], []),
}


def compose(stages):
    from MonsterVariationRoster import CREATURE_IDS, compose as compose_variations
    if any(s['MonsterId'] in CREATURE_IDS for stage in stages for wave in stage['Waves'] for s in wave['MonsterSpawnInfos']):
        return compose_variations(stages)
    result=copy.deepcopy(stages)
    assert set(ROSTER) <= {s['Id'] for s in result}
    for stage in result:
        if stage['Id'] not in ROSTER: continue
        assert len(stage['Waves'])==len(ROSTER[stage['Id']])
        for wave, variants in zip(stage['Waves'],ROSTER[stage['Id']]):
            spawns=wave['MonsterSpawnInfos']
            donors=[s for s in spawns if s['MonsterId'] in CHASERS]
            if not donors:
                assert not variants
                continue
            template=donors[0]
            metadata=lambda s:{k:v for k,v in s.items() if k not in ('MonsterId','SpawnCount')}
            assert all(metadata(s)==metadata(template) for s in donors), 'Mixed chaser spawn timing needs explicit authoring'
            budget=sum(s['SpawnCount'] for s in donors)
            assert budget>=len(variants), (stage['Id'],budget,variants)
            counts=Counter(variants)
            if budget>len(variants): counts[15101]=budget-len(variants)
            replacement=[dict(template,MonsterId=eid,SpawnCount=count) for eid,count in sorted(counts.items())]
            authored=[]
            inserted=False
            for spawn in spawns:
                if spawn['MonsterId'] in CHASERS:
                    if not inserted: authored.extend(replacement); inserted=True
                else: authored.append(spawn)
            wave['MonsterSpawnInfos']=authored
            assert sum(s['SpawnCount'] for s in spawns)==sum(s['SpawnCount'] for s in authored)
    return result


def validate_roster(stages):
    from MonsterVariationRoster import CREATURE_IDS, validate_roster as validate_variations
    if any(s['MonsterId'] in CREATURE_IDS for stage in stages for wave in stage['Waves'] for s in wave['MonsterSpawnInfos']):
        return validate_variations(stages)
    assert compose(stages)==stages, 'Saved P09 roster differs from authored substitutions'
    for stage in stages:
        if stage['Id'] not in ROSTER: continue
        for wave,variants in zip(stage['Waves'],ROSTER[stage['Id']]):
            actual=Counter()
            for s in wave['MonsterSpawnInfos']:
                if s['MonsterId'] in P09_IDS: actual[s['MonsterId']]+=s['SpawnCount']
            assert actual==Counter(variants), (stage['Id'],actual,variants)


def role_id(enemy_id):
    from MonsterVariationRoster import ROLE_IDS
    return ROLE_IDS.get(enemy_id,enemy_id)
