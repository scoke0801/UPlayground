"""Idempotent stage 1/2 asset migration. Original tables are backed up before saving."""
import json
import os
import shutil
import unreal

root = unreal.Paths.project_dir()
backup = os.path.join(unreal.Paths.project_saved_dir(), "Backups", "Stage12")
os.makedirs(backup, exist_ok=True)

def preserve(asset_path):
    relative = asset_path.removeprefix("/Game/") + ".uasset"
    source = os.path.join(root, "Content", relative)
    dest = os.path.join(backup, relative)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    if os.path.exists(source) and not os.path.exists(dest):
        shutil.copy2(source, dest)

def write_table(path, rows):
    preserve(path)
    table = unreal.load_asset(path)
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False)):
        raise RuntimeError("Could not import " + path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + path)

reward_path = "/Game/DataCenter/DataTables/Reward/DT_StatReward"
rewards = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(reward_path)))
if not rewards:
    rewards = [
        {"Name": "PG_MaxHealth", "StatId": 1, "DisplayName": "최대 체력", "StatType": "Health", "Amount": 100},
        {"Name": "PG_Attack", "StatId": 2, "DisplayName": "공격력", "StatType": "Attack", "Amount": 20},
        {"Name": "PG_Defense", "StatId": 3, "DisplayName": "방어력", "StatType": "Defense", "Amount": 30},
    ]
    write_table(reward_path, rewards)
valid_ids = {row["StatId"] for row in rewards}
stage_path = "/Game/DataCenter/DataTables/Stage/DT_StageData"
stages = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(stage_path)))
changed = False
for stage in stages:
    old = stage.get("RewardPool", [])
    if not old or any(r["RewardType"] != "Stat" or r["RewardId"] not in valid_ids for r in old):
        stage["RewardPool"] = [{"RewardType": "Stat", "RewardId": row["StatId"], "Weight": 1.0} for row in rewards[:3]]
        changed = True
if changed:
    write_table(stage_path, stages)

# Create editable presets without changing existing authored montage/VFX content.
tools = unreal.AssetToolsHelpers.get_asset_tools()
def preset(name, cls):
    path = "/Game/DataCenter/Stage12/" + name
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if asset is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls)
        asset = tools.create_asset(name, "/Game/DataCenter/Stage12", cls, factory)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    return asset
camera = preset("DA_PGQuarterView", unreal.PGQuarterViewData)
feedback = preset("DA_PGCombatFeedback", unreal.PGCombatFeedbackData)
tuning = preset("DA_PGCombatTuning", unreal.PGCombatTuningData)
player_path = "/Game/Blueprints/Actor/LocalPlayer/BP_LocalPlayer"
preserve(player_path)
player_class = unreal.EditorAssetLibrary.load_blueprint_class(player_path)
player = unreal.get_default_object(player_class)
player.set_editor_property("quarter_view_data", camera)
player.set_editor_property("feedback_data", feedback)
player.get_editor_property("ability_system_component").set_editor_property("combat_tuning", tuning)
unreal.EditorAssetLibrary.save_asset(player_path, only_if_is_dirty=False)
unreal.log("PG stage 1/2 asset migration complete. Backup: " + backup)