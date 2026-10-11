"""Guard against false-positive QA reports; does not launch Unreal."""
import unittest
from RunQA import check_automation, check_cycle, check_loot_cycle, check_retry, check_telemetry, unexpected_errors


class QAEvidenceTests(unittest.TestCase):
    def test_inventory_resume_requires_one_clean_matching_phase(self):
        from RunInventoryCycle import check_phase
        valid='LogTemp: Display: PGInventoryU8 phase=u8resume failures=0 revision=23'
        self.assertEqual(check_phase(valid,'u8resume'),[])
        for invalid in ('', valid.replace('failures=0','failures=1'), valid+valid,
                        valid+'\nPGInventoryU8 FAIL checkpoint', valid.replace('u8resume','u8seed')):
            self.assertTrue(check_phase(invalid,'u8resume'))

    def test_imported_plant_drop_is_required_after_wave_replacement(self):
        from MonsterVariationRoster import SPEC, CREATURE_COMBAT
        guaranteed={i for grade in SPEC['p09_grades'] if grade['grade']=='Elite' for i in grade['ids']}
        guaranteed.update(c['id'] for c in SPEC['creatures'] if c['base'] in (15104,15105))
        guaranteed.update(c['id'] for c in CREATURE_COMBAT['monsters'] if c['base'] in (15104,15105))
        import copy
        waves=copy.deepcopy(SPEC['waves'])
        for patch in CREATURE_COMBAT['wave_replacements']:
            pack=waves[str(patch['stage'])][patch['wave']-1]
            next(pair for pair in pack if pair[0]==patch['source'])[1]-=1
            pack.append([patch['enemy'],1])
        ids=[eid for stage in waves.values() for wave in stage for eid,count in wave if eid in guaranteed for _ in range(count)]+[15601]
        rows=[f'PGLoot rolled enemy={eid} pool={"Rogue.Boss" if eid==15601 else "Rogue.Warden"} seed=1 item=15004 guid={index:032X} result={int(eid==15601)}'
              for index,eid in enumerate(ids,1)]
        rows+=['PGCombatBT started pawn=BP_15502_C_0',f'PGLoot boss committed item=15004 guid={len(ids):032X} wins=1']
        self.assertEqual(check_loot_cycle('\n'.join(rows)),[])
        self.assertTrue(check_loot_cycle('\n'.join(row for row in rows if 'enemy=15502 ' not in row)))
        plant=next(row for row in rows if 'enemy=15502 ' in row)
        self.assertTrue(check_loot_cycle('\n'.join(rows+[plant])))

    def test_loot_requires_guaranteed_drops_and_atomic_boss_result(self):
        pools={15104:'Rogue.Crusher',15105:'Rogue.Warden',15106:'Rogue.Boss'}
        rows = [f'PGLoot rolled enemy={eid} pool={pools[eid]} seed=1 item=15004 guid={index:032X} result={int(eid==15106)}'
                for index, eid in enumerate((15104, 15105, 15104, 15105, 15106), 1)]
        rows.append(f'PGLoot boss committed item=15004 guid={5:032X} wins=1')
        complete = '\n'.join(rows)
        self.assertEqual(check_loot_cycle(complete), [])
        self.assertTrue(check_loot_cycle('\n'.join(rows[:-1])))
        self.assertTrue(check_loot_cycle('\n'.join(rows[1:])))
        self.assertTrue(check_loot_cycle(complete + '\n' + rows[0]))
        self.assertTrue(check_loot_cycle(complete.replace('wins=1', 'wins=2')))
        self.assertTrue(check_loot_cycle(complete.replace('result=0','result=1',1)))

    def test_normal_p09_ids_are_not_mistaken_for_guaranteed_elites(self):
        pools={15104:'Rogue.Crusher',15105:'Rogue.Warden',15106:'Rogue.Boss',15201:'Rogue.Chaser'}
        rows=[f'PGLoot rolled enemy={eid} pool={pools[eid]} seed=1 item=15004 guid={index:032X} result={int(eid==15106)}'
              for index,eid in enumerate((15104,15105,15104,15105,15106,15201),1)]
        rows.append(f'PGLoot boss committed item=15004 guid={5:032X} wins=1')
        self.assertEqual(check_loot_cycle('\n'.join(rows)),[])

    def test_humanoid_boss_loot_is_committed_exactly_once(self):
        pools={15104:'Rogue.Crusher',15105:'Rogue.Warden',15401:'Rogue.Boss'}
        rows=[f'PGLoot rolled enemy={eid} pool={pools[eid]} seed=1 item=15004 guid={index:032X} result={int(eid==15401)}'
              for index,eid in enumerate((15104,15105,15104,15105,15401),1)]
        rows.append('PGLoot boss committed item=15004 guid='+f'{5:032X}'+' wins=1')
        complete='\n'.join(rows)
        for boss_id in (15401, 15601):
            with self.subTest(boss_id=boss_id):
                current=complete.replace('enemy=15401',f'enemy={boss_id}')
                self.assertEqual(check_loot_cycle(current),[])
                self.assertTrue(check_loot_cycle('\n'.join(current.splitlines()[:-1])))
                self.assertTrue(check_loot_cycle(current+'\n'+current.splitlines()[-2]))
                self.assertTrue(check_loot_cycle(current.replace('wins=1','wins=2')))
        self.assertTrue(check_loot_cycle(complete.replace('enemy=15401','enemy=99999')))
        self.assertTrue(check_loot_cycle(complete+'\n'+rows[-2].replace('15401','15601')))

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
