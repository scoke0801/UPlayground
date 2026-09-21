"""Read-only catalog, skill and Blueprint compatibility gate."""
import unreal
import json
import math

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

data = unreal.load_asset("/Game/DataCenter/Progression/DA_PGProgression")
require(data is not None, "Missing progression catalog")
skill_tables = []
for path in unreal.EditorAssetLibrary.list_assets("/Game/DataCenter", recursive=True):
    if "DT_Skill." in path or path.endswith("/DT_Skill"):
        skill_tables.append(unreal.load_asset(path))
require(len(skill_tables) == 1, "Expected exactly one DT_Skill")
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(skill_tables[0]))
skill_ids = {row["SkillID"] for row in rows}
items = data.get_editor_property("items")
ids = set()
for item in items:
    item_id = item.get_editor_property("id")
    require(item_id > 0 and item_id not in ids, f"Items.Id duplicate/invalid: {item_id}")
    ids.add(item_id)
    require(str(item.get_editor_property("display_name")), f"Item {item_id}: empty DisplayName")
    require(item.get_editor_property("base_options"), f"Item {item_id}: empty BaseOptions")
    require(item.get_editor_property("drop_weight") >= 0, f"Item {item_id}: negative DropWeight")
build_ids = set()
for build in data.get_editor_property("builds"):
    build_id = str(build.get_editor_property("id"))
    require(build_id not in build_ids and build_id != "None", f"Duplicate/empty build: {build_id}")
    build_ids.add(build_id)
    slots = set()
    for entry in build.get_editor_property("skills"):
        slot = entry.get_editor_property("slot")
        skill_id = entry.get_editor_property("skill_id")
        require(slot not in slots, f"{build_id}: duplicate slot {slot}")
        require(skill_id in skill_ids, f"{build_id}/{slot}: unknown skill {skill_id}; known IDs={skill_ids}")
        scale = entry.get_editor_property("cooldown_scale")
        seconds = entry.get_editor_property("cooldown_seconds")
        require(math.isfinite(seconds) and -1 <= seconds <= 300, f"{build_id}/{slot}: invalid cooldown seconds")
        if build_id in {"Rapid1", "Rapid2", "Heavy1", "Heavy2"} and 1 <= int(slot.value) <= 6:
            require(seconds > 0, f"{build_id}/{slot}: authored active cooldown missing")
        require(math.isfinite(scale) and .1 <= scale <= 10, f"{build_id}/{slot}: invalid cooldown scale")
        slots.add(slot)
for path in ["/Game/Blueprints/Actor/LocalPlayer/BP_LocalPlayer"]:
    require(unreal.EditorAssetLibrary.load_blueprint_class(path), f"Blueprint failed to load: {path}")
unreal.log(f"PG Stage34 validated: items={len(items)}, builds={len(build_ids)}, skills={len(skill_ids)}")
