"""Reject misleading latency, cast attribution and proc-cap evidence."""
import unittest
from HackSlashMetrics import parse_metrics, check_spatial_observations


def fixture(profile='1'):
    return '\n'.join('LogTemp: Display: PGSkillMetric '+line for line in [
        f'BEGIN cast=A skill=111 world=10.1 input=10 profile={profile}',
        'HIT cast=A skill=111 world=10.28 phase=0 target=Enemy0 damage=90 behind=0',
        'HIT cast=A skill=111 world=10.5 phase=1 target=Enemy0 damage=90 behind=0',
        'END cast=A skill=111 world=10.82 cancelled=0 displacement=450 hits=2 damage=180 queries=16 frenzy=3 shock=1 refund=0'])


class MetricsTest(unittest.TestCase):
    def parse(self, log, variant='p0'):
        return parse_metrics(log, dict(metrics='1', world='10', variant=variant),
                             [dict(target='0',loss='180'),dict(target='-1',loss='12',time='.3',active_cast='A')],
                             [dict(skill='111')])

    def test_effective_hits_latency_movement_and_incoming(self):
        result, errors = self.parse(fixture())
        self.assertEqual(errors, [])
        skill = result['skills'][0]
        self.assertEqual(skill['effective_hit_ratio'], 1)
        self.assertAlmostEqual(skill['first_press_latency_median'], .28)
        self.assertEqual(skill['both_phases_hit_ratio'], 1)
        self.assertEqual(skill['displacement_cm'], [450])
        self.assertEqual(skill['damage_taken'], 12)

    def test_held_cast_is_not_fresh_press_latency(self):
        result, errors = self.parse(fixture().replace('input=10 ', 'input=-1 '))
        self.assertEqual(errors, [])
        self.assertIsNone(result['skills'][0]['first_press_latency_median'])

    def test_cast_failures(self):
        for old, new in [('damage=180', 'damage=181'), ('frenzy=3','frenzy=4'), ('shock=1','shock=2'),
                         ('refund=0','refund=2'), ('phase=1','phase=0'), ('phase=1','phase=2'),
                         ('world=10.28','world=nan'), ('input=10 ','input=11 '),
                         ('hits=2','hits=1'), ('displacement=450','displacement=-1')]:
            with self.subTest(new=new):
                self.assertTrue(self.parse(fixture().replace(old,new))[1])

    def test_missing_end_or_begin(self):
        lines=fixture().splitlines()
        self.assertTrue(self.parse('\n'.join(lines[:-1]))[1])
        self.assertTrue(self.parse('\n'.join(lines[1:]))[1])

    def test_duplicate_begin(self):
        self.assertTrue(self.parse(fixture().splitlines()[0]+'\n'+fixture())[1])

    def test_legacy_variants_have_no_profile_caps(self):
        result, errors=self.parse(fixture('0').replace('phase=0','phase=-1').replace('phase=1','phase=-1')
                                  .replace('queries=16 frenzy=3 shock=1 refund=0','queries=-1 frenzy=-1 shock=-1 refund=-1'), 'baseline')
        self.assertEqual(errors, [])
        self.assertIsNone(result['skills'][0]['both_phases_hit_ratio'])

    def test_projectile_hit_after_next_skill_keeps_original_identity(self):
        rows=[
            'BEGIN cast=A skill=114 world=10.1 input=10 profile=1',
            'BEGIN cast=B skill=113 world=10.4 input=10.3 profile=1',
            'HIT cast=A skill=114 world=10.6 phase=0 target=Enemy0 damage=160 behind=0',
            'END cast=A skill=114 world=10.7 cancelled=1 displacement=0 hits=1 damage=160 queries=20 frenzy=0 shock=0 refund=0',
            'END cast=B skill=113 world=10.9 cancelled=0 displacement=60 hits=0 damage=0 queries=36 frenzy=0 shock=0 refund=0']
        log='\n'.join('LogTemp: Display: PGSkillMetric '+r for r in rows)
        result,errors=parse_metrics(log,dict(metrics='1',world='10',variant='p1'),[dict(target='0',loss='160')],[dict(skill='114'),dict(skill='113')])
        self.assertEqual(errors,[])
        self.assertEqual(result['casts'][0]['direct_damage'],160)
        self.assertEqual(result['casts'][1]['direct_damage'],0)
        self.assertTrue(parse_metrics(log.replace('HIT cast=A','HIT cast=B'),dict(metrics='1',world='10',variant='p1'),[],[])[1])

    def test_legacy_log_does_not_claim_metric_coverage(self):
        result, errors=parse_metrics('', {}, [], [])
        self.assertEqual(errors, [])
        self.assertEqual(result['status'], 'UNAVAILABLE')

    def test_spatial_uses_independent_hp_and_resolved_contacts(self):
        lines = []
        for skill, count in [(100,1),(101,1),(102,1),(111,4),(112,6)]:
            lines.append(f'LogTemp: PGSkillMetric BEGIN cast=C{skill} skill={skill} world=1 input=1 profile=1')
            for phase in range(count):
                lines.append(f'LogTemp: PGSkillMetric HIT cast=C{skill} skill={skill} world=2 phase={phase} target=Enemy0 damage=10 behind=0')
            lines.append(f'LogTemp: PGSkillMetric END cast=C{skill} skill={skill} world=3 cancelled=0 displacement=0 hits={count} damage={count*10} queries=12 frenzy=0 shock=0 refund=0')
            for target in (0,1):
                damage = count*10 if target == 0 else 0
                lines.append(f'LogTemp: PGHackSlashProbe Skill={skill} Target={target} Damage={damage} Expected={damage} Move=0 Phases={count}')
        log = '\n'.join(lines)
        self.assertEqual(check_spatial_observations(log)['errors'], [])
        for old, new in [('Target=1','Target=0'), ('Move=0','Move=20'),
                         ('Expected=60','Expected=50'), ('Phases=6','Phases=5'),
                         ('Damage=60','Damage=nan'), ('displacement=0','displacement=nan'),
                         ('cancelled=0','cancelled=1')]:
            with self.subTest(new=new):
                self.assertEqual(check_spatial_observations(log.replace(old,new))['status'], 'FAIL')


if __name__ == '__main__':
    unittest.main()
