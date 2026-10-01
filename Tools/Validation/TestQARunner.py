"""Guard against false-positive QA reports; does not launch Unreal."""
import unittest
from RunQA import check_automation, check_cycle, check_retry, check_telemetry, unexpected_errors


class QAEvidenceTests(unittest.TestCase):
    def test_telemetry_requires_assisted_commits_and_finite_samples(self):
        rows = [dict(stage=stage, run_seed=173001, assisted=True, outcome="Cleared",
                     selected_rewards=[15000] * count, combat_seconds=10,
                     direct_damage=0, secondary_damage=0, damage_taken=5)
                for stage, count in enumerate([1, 2, 1, 2, 1, 0], 1)]
        report = dict(schema_version=1, stages=rows)
        self.assertEqual(check_telemetry(report), [])
        rows[0]["assisted"] = False
        self.assertTrue(check_telemetry(report))
        rows[0]["assisted"] = True
        rows[0]["selected_rewards"] = []
        self.assertTrue(check_telemetry(report))
        rows[0]["selected_rewards"] = [15000]
        rows[0]["direct_damage"] = float("nan")
        self.assertTrue(check_telemetry(report))
        self.assertTrue(check_telemetry({}))

    def test_automation_requires_discovered_tests_and_finished_results(self):
        report = dict(succeeded=1, failed=0, notRun=0, inProcess=0,
                      tests=[dict(fullTestPath="PG.Example", state="Success", errors=0)])
        self.assertEqual(check_automation(report, ["PG.Example"])[0], [])
        self.assertTrue(check_automation(report, ["PG.Example", "PG.Missing"])[0])
        report["tests"][0]["state"] = "InProcess"
        self.assertTrue(check_automation(report, ["PG.Example"])[0])

    def test_zero_tests_cannot_pass(self):
        self.assertTrue(check_automation(dict(failed=0, notRun=0, inProcess=0, tests=[]), [])[0])

    def test_warning_is_preserved(self):
        test = dict(fullTestPath="PG.Example", state="Success", errors=0,
                    entries=[dict(event=dict(type="Warning", message="Warning detail"))])
        errors, warnings = check_automation(dict(failed=0, notRun=0, inProcess=0, tests=[test]), ["PG.Example"])
        self.assertFalse(errors)
        self.assertEqual(warnings, ["PG.Example: Warning detail"])

    def test_cycle_requires_all_waves_and_committed_rewards(self):
        lines = [f"PGWave started stage={stage} wave={wave}/3" for stage in range(1, 6) for wave in range(1, 4)]
        lines += ["PGWave started stage=6 wave=1/1", "PGCombatCycle COMPLETE rewards=7 attack=320"]
        for stage in [1, 2, 2, 3, 4, 4, 5]:
            lines += [f"PGCombatCycle select stage={stage}"]
        lines += [f"PGReward applied stage={stage}" for stage in range(1, 6)]
        complete = "\n".join(lines)
        self.assertEqual(check_cycle(complete), [])
        self.assertEqual(check_cycle(complete + "\nLogInit: Command Line: -TestExit=PGCombatCycle COMPLETE+PGCombatCycle FAILED"), [])
        self.assertTrue(check_cycle("LogInit: Command Line: -TestExit=PGCombatCycle COMPLETE rewards=7"))
        self.assertTrue(check_cycle("PGCombatCycle COMPLETE rewards=7 attack=320"))
        self.assertTrue(check_cycle(complete.replace("PGReward applied stage=5", "")))
        self.assertTrue(check_cycle(complete + "\nPGWave started stage=1 wave=1/3"))
        self.assertTrue(check_cycle(complete + "\nPGCombatCycle reward rejected"))

    def test_retry_requires_all_restarts(self):
        complete = "\n".join(f"PGRetryProbe remaining={n} healthy=1 failures=0" for n in range(20, -1, -1))
        complete += "\nLogTemp: Error: Stage 1 failed: Player defeated." * 20
        complete += "\nPGRetryProbe COMPLETE failures=0"
        self.assertEqual(check_retry(complete), [])
        self.assertTrue(check_retry("PGRetryProbe COMPLETE failures=0"))
        self.assertTrue(check_retry(complete.replace("remaining=10 healthy=1", "remaining=10 healthy=0")))
        self.assertTrue(check_retry(complete + "\nLogTemp: Error: Stage 1 failed: Player defeated."))

    def test_expected_death_is_only_allowed_in_retry(self):
        log = "LogTemp: Error: Stage 1 failed: Player defeated."
        self.assertEqual(unexpected_errors(log, "retry"), [])
        self.assertTrue(unexpected_errors(log, "cycle"))
        self.assertTrue(unexpected_errors(log + "\nLogTemp: Error: Spawn retries exhausted: 15101", "retry"))


if __name__ == "__main__":
    unittest.main()
