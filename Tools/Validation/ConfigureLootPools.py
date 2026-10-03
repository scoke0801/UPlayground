"""Add role-specific loot without resetting combat, items, equipment or reward tuning."""
import copy
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir())
sys.path.insert(0, str(ROOT / 'Tools/Validation'))
from ValidateLootPools import validate_loot_pools

CATALOG = '/Game/DataCenter/Progression/DA_PGProgression'
ENEMIES = '/Game/DataCenter/DataTables/Actor/DT_Enemy'
backup = ROOT / 'Saved/Backups/LootPools' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
for asset_path in (CATALOG, ENEMIES):
    relative = Path(asset_path.removeprefix('/Game/') + '.uasset')
    target = backup / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / 'Content' / relative, target)

catalog = unreal.load_asset(CATALOG)
table = unreal.load_asset(ENEMIES)
assert catalog and table
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
before = copy.deepcopy(rows)
definitions = {
    15101: ('Rogue.Chaser', False, {15002: 6, 15003: 2, 15004: 1, 15006: 1}),
    15102: ('Rogue.Shooter', False, {15002: 2, 15003: 6, 15005: 2}),
    15103: ('Rogue.Guardian', False, {15002: 2, 15003: 5, 15004: 1, 15005: 1, 15006: 1}),
    15104: ('Rogue.Crusher', True, {15002: 3, 15003: 2, 15004: 3, 15006: 3}),
    15105: ('Rogue.Warden', True, {15002: 2, 15003: 3, 15005: 5}),
    15106: ('Rogue.Boss', True, {15004: 1, 15005: 1, 15006: 1}),
}
assert set(definitions) <= {r['EnemyID'] for r in rows}
items = {i.get_editor_property('id') for i in catalog.get_editor_property('items')}
pools = list(catalog.get_editor_property('drop_pools'))
pool_ids = {str(p.get_editor_property('id')) for p in pools}
for eid, (pool_id, guaranteed, weights) in definitions.items():
    assert set(weights) <= items
    if pool_id not in pool_ids:
        pool = unreal.PGDropPool()
        pool.set_editor_property('id', pool_id)
        pool.set_editor_property('guaranteed', guaranteed)
        pool.set_editor_property('drop_chance', catalog.get_editor_property('drop_chance'))
        entries = []
        for item_id, weight in weights.items():
            entry = unreal.PGDropPoolEntry()
            entry.set_editor_property('item_id', item_id)
            entry.set_editor_property('weight', float(weight))
            entries.append(entry)
        pool.set_editor_property('entries', entries)
        pools.append(pool)
    row = next(r for r in rows if r['EnemyID'] == eid)
    assert row.get('DropPoolId', 'None') in ('None', pool_id), f'Preserve custom pool on {eid}'
    row['DropPoolId'] = pool_id

for old, new in zip(before, rows):
    old.pop('DropPoolId', None)
    other = dict(new)
    other.pop('DropPoolId', None)
    assert old == other, 'Combat data changed'
catalog.set_editor_property('drop_pools', pools)
validate_loot_pools(catalog, {r['EnemyID']: r for r in rows})
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False))
assert unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False)
assert unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False)
unreal.log('PGLootPools MIGRATION PASS pools=6 backup=' + str(backup))
