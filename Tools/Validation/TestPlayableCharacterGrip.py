"""Failure-oriented tests for the combat comparison acceptance gate."""
import copy
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from RunPlayableCharacterGrip import SKILLS, inspect, compare, review


def fixture(stress=False):
    lines = [f'PGGrip Setup Identity=Bokusei Stress={int(stress)} Moved={10 if stress else 0}',
             'PGHackSlashProbe PASS skills=8 grip=1']
    for index, skill in enumerate(SKILLS):
        start = index*3+1
        lines += [f'PGSkillMetric BEGIN cast=c{index} skill={skill} world={start} input={start} profile=1',
                  f'PGSkillMetric HIT cast=c{index} skill={skill} phase=0 target=Target world={start+.2} damage=10 behind=0',
                  f'PGSkillMetric END cast=c{index} skill={skill} world={start+1} cancelled=0 displacement=0 hits=1 damage=10 queries=1 frenzy=0 shock=0 refund=0',
                  f'PGGrip Collision Skill={skill} Requested=1 Effective=0 Time=0.2 Type=0',
                  f'PGGrip Notify Skill={skill} Begin=1 Source=1 Time=0.2']
        for target in (0, 1):
            damage = 10 if target == 0 else 0
            lines.append(f'PGHackSlashProbe Skill={skill} Target={target} Damage={damage} Expected={damage} Move=0 Phases=1')
        for phase in range(3 if skill == 113 else 2 if skill in (111, 112) else 1):
            lines.append(f'PGSkill Presentation Skill={skill} Phase={phase} Time=0.2 X=0 Y=0 Z=0 Yaw=0 Projectile={int(skill==114)}')
            for _ in range(2 if skill in (110,112) else 1):
                lines.append(f'PGGrip VFX Skill={skill} Available=1 X=0 Y=0 Z=0 Pitch=0 Yaw=0 Roll=0 Radius=100 Reverse=0')
        for sample in range(5):
            lines.append(f'PGGrip Sample Skill={skill} Time={sample*.1} X={10 if stress else 0} Y=0 Z=0 QX=0 QY=0 QZ=0 QW=1')
    return '\n'.join('LogTemp: Display: '+line for line in lines)


class GripGateTests(unittest.TestCase):
    def setUp(self):
        self.before = inspect(fixture(), 'Bokusei', False)
        self.after = inspect(fixture(True), 'Bokusei', True)

    def test_unchanged_combat_accepts_real_stimulus(self):
        self.assertEqual(compare(self.before, self.after), [])

    def test_damage_timing_and_displacement_regressions_fail(self):
        for field, value in [('direct_damage', 20), ('end', 4), ('displacement_cm', 1)]:
            changed = copy.deepcopy(self.after)
            changed['casts'][0][field] = value
            self.assertTrue(compare(self.before, changed), field)

    def test_missing_or_duplicate_events_fail(self):
        for section in ('casts', 'targets', 'notifies', 'vfx', 'presentation', 'collisions'):
            changed = copy.deepcopy(self.after)
            changed[section].append(copy.deepcopy(changed[section][0]))
            self.assertTrue(compare(self.before, changed), section)

    def test_fx_origin_and_notify_source_changes_fail(self):
        self.after['vfx'][0]['X'] = '1'
        self.assertTrue(compare(self.before, self.after))
        log = fixture(True).replace('Source=1', 'Source=0')
        self.assertTrue(inspect(log, 'Bokusei', True)['errors'])

    def test_noop_stimulus_during_attacks_fails(self):
        self.after['attachment_samples'] = self.before['attachment_samples']
        self.assertTrue(compare(self.before, self.after))

    def test_missing_vfx_nonfinite_pose_and_enabled_legacy_fail(self):
        for old, new in [('PGGrip VFX', 'Ignored VFX'), ('X=10', 'X=nan'), ('Effective=0', 'Effective=1')]:
            self.assertTrue(inspect(fixture(True).replace(old, new), 'Bokusei', True)['errors'])

    def test_review_preserves_process_failures_and_requires_complete_scope(self):
        with tempfile.TemporaryDirectory() as directory, contextlib.redirect_stdout(io.StringIO()):
            root = Path(directory)
            original = dict(status='FAIL', comparisons=[dict(identity='Bokusei')], requested_identities=['Bokusei'])
            for stress in (False, True):
                folder = root/('Bokusei_stress' if stress else 'Bokusei_baseline')
                folder.mkdir()
                (folder/'engine.log').write_text(fixture(stress), encoding='utf-8')
                (folder/'observations.json').write_text(json.dumps(dict(errors=[])), encoding='utf-8')
            report = root/'report.json'
            report.write_text(json.dumps(original), encoding='utf-8')
            before = report.read_bytes()
            self.assertEqual(review(root), 0)
            self.assertEqual(report.read_bytes(), before)
            for failure in ('Process exit=1, timeout=False', 'Frozen module was not loaded: PGActor'):
                (root/'Bokusei_stress/observations.json').write_text(json.dumps(dict(errors=[failure])), encoding='utf-8')
                self.assertEqual(review(root), 1)
            original['requested_identities'].append('Honoka')
            report.write_text(json.dumps(original), encoding='utf-8')
            self.assertEqual(review(root), 1)


if __name__ == '__main__':
    unittest.main()
