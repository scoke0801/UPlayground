"""Validate both generic pool integrity and the authored first loot milestone."""
import math


def validate_loot_pools(catalog, enemies):
    items = {i.get_editor_property('id'): i for i in catalog.get_editor_property('items')}
    pools = {}
    for pool in catalog.get_editor_property('drop_pools'):
        name = str(pool.get_editor_property('id'))
        assert name != 'None' and name not in pools, ('Duplicate/empty drop pool', name)
        pools[name] = pool
        chance = pool.get_editor_property('drop_chance')
        assert math.isfinite(chance) and 0 <= chance <= 1, name
        ids, total = set(), 0.
        for entry in pool.get_editor_property('entries'):
            iid, weight = entry.get_editor_property('item_id'), entry.get_editor_property('weight')
            assert iid in items and iid not in ids, (name, iid)
            assert math.isfinite(weight) and weight >= 0, (name, iid, weight)
            assert items[iid].get_editor_property('base_options'), (name, iid)
            ids.add(iid)
            total += weight
        assert math.isfinite(total) and 0 < total <= 3.402823466e38, name
    for row in enemies.values():
        name = row.get('DropPoolId', 'None')
        assert name == 'None' or name in pools, (row['EnemyID'], name)
    for eid, suffix in zip(range(15101, 15107), ('Chaser', 'Shooter', 'Guardian', 'Crusher', 'Warden', 'Boss')):
        name = 'Rogue.' + suffix
        assert enemies[eid]['DropPoolId'] == name and name in pools, (eid, name)
        assert pools[name].get_editor_property('guaranteed') == (eid >= 15104), name
    boss_ids = {e.get_editor_property('item_id') for e in pools['Rogue.Boss'].get_editor_property('entries') if e.get_editor_property('weight') > 0}
    assert boss_ids and all(int(items[i].get_editor_property('rarity').value) >= 2 for i in boss_ids), 'Boss rewards must be rare equipment'
    return 1
