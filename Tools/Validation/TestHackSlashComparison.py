"""Protect comparison evidence from incomplete trials and accidental smoke acceptance."""
import unittest
import json
from pathlib import Path
import tempfile
from RunHackSlashComparison import parse_trial, baseline_evidence, DEFAULT_BASELINE, summarize_trials


def fixture(outcome='timeout', smoke=False):
    lines = [f'ROW skill={i} profile=1 cooldown=0' for i in (100, 101, 102, 111, 112)]
    lines += ['SPAWN index=0 enemy=15104 hp=5000.0 x=0 y=0 z=0',
             f'BEGIN scenario=P0-E1 seed=173001 variant=p0 enemies=1 attack=100.0 hp=1000.0 combat_assistance=0 contact_injected=0 smoke={int(smoke)}',
             'USE skill=100 time=0.1', 'HEALTH target=0 enemy=15104 time=0.3 loss=90 hp=4910',
             'HEALTH target=-1 enemy=0 time=0.5 loss=10 hp=990',
             f'END outcome={outcome} seconds=30 damage=90 taken=10 kills=0 uses=1 smoke={int(smoke)}']
    log = '\n'.join('LogTemp: Display: PGSkillTrial ' + line for line in lines)
    if outcome == 'death':
        log = log.replace('loss=10 hp=990', 'loss=1000 hp=0').replace('taken=10 ', 'taken=1000 ')
    return log


class ComparisonEvidenceTest(unittest.TestCase):
    def parse(self, log, smoke=False):
        return parse_trial(log, 'P0-E1', 173001, 'p0', smoke)

    def test_timeout_and_death_are_retained(self):
        for outcome in ('timeout', 'death'):
            report = self.parse(fixture(outcome))
            self.assertEqual(report['status'], 'RECORDED')
            self.assertFalse(report['p0_acceptance_complete'])
            self.assertFalse(report['direct_input_verified'])

    def test_aborted_trial_fails(self):
        self.assertEqual(self.parse(fixture().split('LogTemp: Display: PGSkillTrial END')[0])['status'], 'FAIL')

    def test_damage_must_reconcile(self):
        self.assertEqual(self.parse(fixture().replace('damage=90', 'damage=900'))['status'], 'FAIL')

    def test_wrong_seed_fails(self):
        self.assertEqual(self.parse(fixture().replace('seed=173001', 'seed=173002'))['status'], 'FAIL')

    def test_variant_must_match_runtime_rows(self):
        self.assertEqual(self.parse(fixture().replace('profile=1', 'profile=0'))['status'], 'FAIL')

    def test_smoke_is_not_manual(self):
        log = fixture('smoke_complete', True)
        self.assertEqual(self.parse(log, True)['status'], 'RECORDED')
        self.assertEqual(self.parse(log)['status'], 'FAIL')

    def test_duplicate_boundaries_fail(self):
        self.assertEqual(self.parse(fixture() + '\n' + fixture())['status'], 'FAIL')

    def test_incomplete_clear_fails(self):
        self.assertEqual(self.parse(fixture('clear'))['status'], 'FAIL')

    def test_short_timeout_fails(self):
        self.assertEqual(self.parse(fixture().replace('seconds=30', 'seconds=3'))['status'], 'FAIL')

    def test_nonfinite_or_inconsistent_health_fails(self):
        for old,new in [('loss=90','loss=nan'),('seconds=30','seconds=inf'),('hp=4910','hp=4900'),
                        ('target=0 enemy=15104 time=0.3','target=5 enemy=15104 time=0.3')]:
            with self.subTest(new=new):
                self.assertEqual(self.parse(fixture().replace(old,new))['status'], 'FAIL')

    def test_event_bounds_and_identity(self):
        for old,new in [('enemy=15104 time=0.3','enemy=15102 time=0.3'),('skill=100 time=0.1','skill=100 time=31'),
                        ('x=0 y=0','x=nan y=0')]:
            self.assertEqual(self.parse(fixture().replace(old,new))['status'],'FAIL')
        self.assertEqual(self.parse(fixture()+'\nLogTemp: Display: PGSkillTrial USE skill=111 time=1')['status'],'FAIL')

    def test_strengthened_build_cannot_fill_base_matrix(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp)/'report.json').write_text(json.dumps(dict(scenario='P0-E1',seed=173001,variant='p0',
                build='frenzy',smoke=False,status='RECORDED')), encoding='utf-8')
            result=summarize_trials(Path(tmp))
            self.assertEqual(len(result['strengthened_reports']),1)
            self.assertTrue(all(c['status']=='PENDING' for c in result['trials']))

    def test_corrupt_report_retained_without_breaking_summary(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp)/'report.json').write_text('{', encoding='utf-8')
            self.assertEqual(len(summarize_trials(Path(tmp))['invalid_reports']),1)

    def test_pinned_baseline_is_pre_migration(self):
        self.assertEqual(len(baseline_evidence(DEFAULT_BASELINE)['skills']), 5)

    def test_matrix_keeps_failures_and_excludes_smoke(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name, smoke, status in [('aborted', False, 'FAIL'), ('death', False, 'RECORDED'), ('smoke', True, 'RECORDED')]:
                folder = Path(tmp) / name
                folder.mkdir()
                (folder / 'report.json').write_text(json.dumps(dict(scenario='P0-E1', seed=173001, variant='p0',
                                                                  smoke=smoke, status=status)), encoding='utf-8')
            report = summarize_trials(Path(tmp))
            self.assertEqual(len(report['trials']), 40)
            self.assertEqual(sum(len(t['attempts']) for t in report['trials']), 2)
            self.assertEqual(len(report['excluded_smoke']), 1)
            self.assertFalse(report['p0_acceptance_complete'])


if __name__ == '__main__':
    unittest.main()
