import copy
import unittest
from MonsterVariationRoster import SPEC,IDS,compose,validate_roster

class MonsterVariationRosterTests(unittest.TestCase):
    def setUp(self):
        self.stages=[dict(Id=int(sid),Reward='preserve',Waves=[dict(Delay=2,MonsterSpawnInfos=[dict(MonsterId=15101,SpawnCount=sum(n for _,n in pack),SpawnPriority=0,SpawnDelayTime=0)]) for pack in waves]) for sid,waves in SPEC['waves'].items()]
        self.stages.append(dict(Id=6,Waves=[dict(MonsterSpawnInfos=[dict(MonsterId=15106,SpawnCount=1)])]))

    def test_composition_is_idempotent_and_preserves_boss_and_metadata(self):
        original=copy.deepcopy(self.stages)
        result=compose(self.stages)
        validate_roster(result)
        self.assertEqual(original,self.stages)
        self.assertEqual(result,compose(result))
        self.assertEqual(original[-1],result[-1])
        self.assertEqual('preserve',result[0]['Reward'])
        self.assertEqual(2,result[0]['Waves'][0]['Delay'])

    def test_unknown_budget_is_refused(self):
        self.stages[0]['Waves'][0]['MonsterSpawnInfos'][0]['SpawnCount']+=1
        with self.assertRaises(AssertionError):compose(self.stages)

    def test_mixed_timing_is_not_silently_discarded(self):
        wave=self.stages[0]['Waves'][0]['MonsterSpawnInfos']
        wave[0]['SpawnCount']-=1
        wave.append(dict(wave[0],MonsterId=15102,SpawnCount=1,SpawnDelayTime=3))
        with self.assertRaises(AssertionError):compose(self.stages)

    def test_tiers_grow_and_unlock_in_order(self):
        for field in ('Health','Attack','Defense'):
            values=[g['stats'][field] for g in SPEC['p09_grades']]
            self.assertTrue(all(a<b for a,b in zip(values,values[1:])))
        first={eid:min(int(sid) for sid,waves in SPEC['waves'].items() if any(eid==i for wave in waves for i,_ in wave)) for eid in IDS}
        for grade,expected in zip(SPEC['p09_grades'],(1,3,4)):
            self.assertEqual([expected,expected],[first[i] for i in grade['ids']])

if __name__=='__main__':unittest.main()
