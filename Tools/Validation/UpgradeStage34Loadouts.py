"""One-time correction: legacy DT_Skill active cooldowns are zero, so scale alone cannot create growth."""
import os
import shutil
import unreal

path = "/Game/DataCenter/Progression/DA_PGProgression"
data = unreal.load_asset(path)
source = os.path.join(unreal.Paths.project_content_dir(), "DataCenter/Progression/DA_PGProgression.uasset")
backup = os.path.join(unreal.Paths.project_saved_dir(), "Backups/Stage34/DA_PGProgression.uasset")
os.makedirs(os.path.dirname(backup), exist_ok=True)
if not os.path.exists(backup):
    shutil.copy2(source, backup)
builds = list(data.get_editor_property("builds"))
for build_index, build in enumerate(builds):
    name = str(build.get_editor_property("id"))
    entries = list(build.get_editor_property("skills"))
    # Keep subsequent designer tuning intact; only migrate the original generated layout.
    original = [100,110,111,112,113,114,115,10000,20000] if name.startswith("Rapid") else [100,114,115,112,113,110,111,10000,20000]
    replacement = [100,113,111,112,110,114,115,10000,20000] if name.startswith("Rapid") else [100,112,115,113,110,114,111,10000,20000]
    replace_ids = [e.get_editor_property("skill_id") for e in entries] == original
    for index, entry in enumerate(entries):
        if replace_ids:
            entry.set_editor_property("skill_id", replacement[index])
        if 1 <= index <= 6 and entry.get_editor_property("cooldown_seconds") < 0:
            entry.set_editor_property("cooldown_seconds", 12. if name.startswith("Heavy") and index == 2 else 8.)
        entries[index] = entry
    build.set_editor_property("skills", entries)
    builds[build_index] = build
data.set_editor_property("builds", builds)
if not unreal.EditorAssetLibrary.save_loaded_asset(data, only_if_is_dirty=False):
    raise RuntimeError("Loadout migration failed")
unreal.log("PG Stage34 authored cooldowns and distinct active layouts migrated")
