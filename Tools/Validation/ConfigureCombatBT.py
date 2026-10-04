"""Create editable role BT/BB assets and bind arena rows, preserving unrelated data.

Existing graphs/tuning are kept unless -PGRebuildCombatBT is explicitly supplied.
Run in a fresh editor after building PGAI. Originals are backed up before saving.
"""
import copy
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import unreal

ROOT = Path(unreal.Paths.project_dir())
BACKUP = ROOT / 'Saved/Backups/CombatBT' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT = '/Game/DataCenter/AI/Combat'
TREE = OUT + '/BT_PGCombatRole'
BOARD = OUT + '/BB_PGCombatRole'
TABLE = '/Game/DataCenter/DataTables/Actor/DT_Enemy'
REBUILD = '-PGRebuildCombatBT' in unreal.SystemLibrary.get_command_line()


def preserve(path):
    relative = path.split('.')[0].removeprefix('/Game/') + '.uasset'
    source, target = ROOT / 'Content' / relative, BACKUP / relative
    if source.is_file() and not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def asset(path, cls, factory):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/', 1)[1], OUT, cls, factory)


existed = unreal.EditorAssetLibrary.does_asset_exist(TREE)
preserve(TREE)
preserve(BOARD)
tree = asset(TREE, unreal.BehaviorTree, unreal.BehaviorTreeFactory())
board = asset(BOARD, unreal.BlackboardData, unreal.BlackboardDataFactory())
assert tree and board
if not existed or REBUILD:
    assert unreal.PGCombatTreeLibrary.configure_editor_tree(tree, board, .2), 'BT graph creation failed'
    assert unreal.EditorAssetLibrary.save_loaded_asset(board, False)
    assert unreal.EditorAssetLibrary.save_loaded_asset(tree, False)
assert unreal.PGCombatTreeLibrary.validate_combat_tree(tree, True), 'Existing graph is incompatible; inspect before rebuilding'

table = unreal.load_asset(TABLE)
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
before = copy.deepcopy(rows)
profiles = {
    15101: (140, 280, .9), 15102: (230, 350, 1.1), 15103: (200, 250, 1.2),
    15104: (250, 300, 1.2), 15105: (260, 350, 1.2), 15106: (280, 280, 1.5),
}
for row in rows:
    if row['EnemyID'] not in profiles:
        continue
    assert row['Role'] != 'Legacy'
    old_tree = row.get('CombatBehaviorTree', '')
    if not old_tree or old_tree == 'None' or REBUILD:
        row['CombatBehaviorTree'] = TREE + '.BT_PGCombatRole'
        separation, travel, interval = profiles[row['EnemyID']]
        row['Positioning'] = dict(bEnabled=True, Separation=separation, MaxMoveDistance=travel,
                                  ReconsiderSeconds=interval, MinimumImprovement=45)
if before != rows:
    preserve(TABLE)
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False))
    assert unreal.EditorAssetLibrary.save_loaded_asset(table, False)
assert sum(r['EnemyID'] in profiles for r in rows) == 6
unreal.log('PGCombatBT CONFIGURED ' + json.dumps(dict(roles=6, tree=TREE, backup=str(BACKUP))))
