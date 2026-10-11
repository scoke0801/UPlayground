"""Spawn-budget, progression metadata and loot-evidence regression contracts."""
import copy
from collections import Counter
import unittest
from MonsterVariationRoster import SPEC as ORIGINAL, CREATURE_COMBAT, compose as compose_all
from P09WeaponRoster import SPEC, IDS, compose, validate_roster
from RunQA import check_loot_cycle


def baseline():
    stages=[dict(Id=int(sid),RewardIds=[15000,15001],Waves=[dict(Delay=1.7,MonsterSpawnInfos=[dict(MonsterId=eid,SpawnCount=count,SpawnInterval=.35) for eid,count in pack]) for pack in waves]) for sid,waves in ORIGINAL['waves'].items()]
    for patch in CREATURE_COMBAT['wave_replacements']:
        infos=stages[patch['stage']-1]['Waves'][patch['wave']-1]['MonsterSpawnInfos']
        source=next(i for i in infos if i['MonsterId']==patch['source']);source['SpawnCount']-=1
        infos[:]=[i for i in infos if i['SpawnCount']]+[dict(source,MonsterId=patch['enemy'],SpawnCount=1)]
    stages.append(dict(Id=6,RewardIds=[15002],Waves=[dict(Delay=3.,MonsterSpawnInfos=[dict(MonsterId=15601,SpawnCount=1,SpawnInterval=.35)])]))
    return stages


class P09WeaponsTests(unittest.TestCase):
    def test_wave_budget_metadata_boss_and_source_are_preserved(self):
        before=baseline();snapshot=copy.deepcopy(before);after=compose(before)
        self.assertEqual(before,snapshot)
        for old,new in zip(before,after):
            self.assertEqual(old['RewardIds'],new['RewardIds'])
            for a,b in zip(old['Waves'],new['Waves']):
                self.assertEqual(a['Delay'],b['Delay'])
                self.assertEqual(sum(i['SpawnCount'] for i in a['MonsterSpawnInfos']),sum(i['SpawnCount'] for i in b['MonsterSpawnInfos']))
                self.assertTrue(all(i['SpawnInterval']==.35 for i in b['MonsterSpawnInfos']))
        self.assertEqual(after[-1],before[-1]);validate_roster(after)

    def test_reapplying_base_and_weapon_generators_keeps_new_roster(self):
        stages=compose(baseline())
        self.assertEqual(compose(stages),stages)
        self.assertEqual(compose_all(stages),stages)
        self.assertEqual(len(IDS),18)
        self.assertEqual(Counter((m['family'],m['grade']) for m in SPEC['monsters']),Counter({(f,g):2 for f in ('Spear','Greatsword','Staff') for g in ('Normal','Veteran','Elite')}))

    def test_rejects_missing_or_duplicated_new_spawns(self):
        stages=compose(baseline())
        info=next(i for s in stages for w in s['Waves'] for i in w['MonsterSpawnInfos'] if i['MonsterId'] in IDS)
        info['SpawnCount']=2
        with self.assertRaises(AssertionError):validate_roster(stages)

    def test_guaranteed_loot_is_preserved_and_missing_new_elite_is_rejected(self):
        guaranteed={i for g in ORIGINAL['p09_grades'] if g['grade']=='Elite' for i in g['ids']}
        guaranteed.update(c['id'] for c in ORIGINAL['creatures'] if c['base'] in (15104,15105))
        guaranteed.update(c['id'] for c in CREATURE_COMBAT['monsters'] if c['base'] in (15104,15105))
        guaranteed.update(m['id'] for m in SPEC['monsters'] if m['grade']=='Elite')
        def drop_ids(stages):return [i['MonsterId'] for s in stages for w in s['Waves'] for i in w['MonsterSpawnInfos'] for _ in range(i['SpawnCount']) if i['MonsterId'] in guaranteed or i['MonsterId']==15601]
        old=drop_ids(baseline());new=drop_ids(compose(baseline()))
        self.assertEqual(len(old),len(new))
        rows=[f'PGLoot rolled enemy={eid} pool={"Rogue.Boss" if eid==15601 else "Rogue.Crusher"} seed=1 item=15004 guid={index:032X} result={int(eid==15601)}' for index,eid in enumerate(new,1)]
        rows+=['PGCombatBT started pawn=BP_15502_C_0',f'PGLoot boss committed item=15004 guid={len(new):032X} wins=1']
        self.assertEqual(check_loot_cycle('\n'.join(rows)),[])
        self.assertTrue(check_loot_cycle('\n'.join(r for r in rows if 'enemy=15705 ' not in r)))
        self.assertTrue(check_loot_cycle('\n'.join(rows+[next(r for r in rows if 'enemy=15705 ' in r)])))


if __name__=='__main__':unittest.main()
