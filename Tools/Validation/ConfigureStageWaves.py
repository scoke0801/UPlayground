"""Migrate legacy stage spawns to up to three sequential waves, with a backup.

Run in UE 5.8 after compiling the updated stage schema. Existing authored Waves
are preserved unless -PGRebuildStageWaves is explicitly supplied. Counts per
monster and all unrelated stage fields are preserved.
"""
import collections
import copy
import json
import os
import shutil
import unreal

PATH = "/Game/DataCenter/DataTables/Stage/DT_StageData"
table = unreal.load_asset(PATH)
if table is None:
    raise RuntimeError("Missing stage table: " + PATH)
original = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
rows = json.loads(original)
changed = False
summary = []
for row in rows:
    if not row.get("Waves") or "-PGRebuildStageWaves" in unreal.SystemLibrary.get_command_line():
        legacy = row.get("MonsterSpawnInfos", [])
        if not legacy or any(s["SpawnCount"] <= 0 or s["MonsterId"] <= 0 for s in legacy):
            raise RuntimeError("Invalid legacy stage: " + str(row["Id"]))
        count = min(3, sum(s["SpawnCount"] for s in legacy))
        waves = [{"MonsterSpawnInfos": [], "StartDelay": 2.0} for _ in range(count)]
        base, extra = divmod(sum(s["SpawnCount"] for s in legacy), count)
        capacities = [base + int(index >= count - extra) for index in range(count)]
        index = 0
        for spawn in legacy:
            remaining = spawn["SpawnCount"]
            while remaining:
                amount = min(remaining, capacities[index])
                entry = copy.deepcopy(spawn)
                entry["SpawnCount"] = amount
                waves[index]["MonsterSpawnInfos"].append(entry)
                remaining -= amount
                capacities[index] -= amount
                if capacities[index] == 0:
                    index += 1
        waves[0]["StartDelay"] = 0.0
        # Delays remain relative to other entries of this wave, not the old stage.
        for wave in waves:
            first = min(s.get("SpawnDelayTime", 0.0) for s in wave["MonsterSpawnInfos"])
            for spawn in wave["MonsterSpawnInfos"]:
                spawn["SpawnDelayTime"] = spawn.get("SpawnDelayTime", 0.0) - first
        before, after = collections.Counter(), collections.Counter()
        for spawn in legacy:
            before[spawn["MonsterId"]] += spawn["SpawnCount"]
        for wave in waves:
            for spawn in wave["MonsterSpawnInfos"]:
                after[spawn["MonsterId"]] += spawn["SpawnCount"]
        if before != after:
            raise RuntimeError("Migration changed monster totals")
        row["Waves"] = waves
        row["BuildDuration"] = 30.0
        changed = True
    summary.append({"stage": row["Id"], "build_seconds": row["BuildDuration"],
                    "wave_counts": [sum(s["SpawnCount"] for s in w["MonsterSpawnInfos"]) for w in row["Waves"]]})

backup = os.path.join(unreal.Paths.project_saved_dir(), "Backups", "StageWaves")
os.makedirs(backup, exist_ok=True)
if changed:
    source = os.path.join(unreal.Paths.project_content_dir(), PATH.removeprefix("/Game/") + ".uasset")
    dest = os.path.join(backup, "DT_StageData.uasset")
    if not os.path.exists(dest):
        shutil.copy2(source, dest)
        with open(os.path.join(backup, "DT_StageData.before.json"), "w", encoding="utf-8") as output:
            output.write(original)
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False)):
        raise RuntimeError("Could not import wave table")
    if not unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False):
        raise RuntimeError("Could not save wave table")
actual = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
if actual != rows:
    raise RuntimeError("Wave table round-trip mismatch")
with open(os.path.join(backup, "WaveSummary.json"), "w", encoding="utf-8") as output:
    json.dump(summary, output, ensure_ascii=False, indent=2)
unreal.log("PGWave migration PASS changed=" + str(changed) + " " + json.dumps(summary))
