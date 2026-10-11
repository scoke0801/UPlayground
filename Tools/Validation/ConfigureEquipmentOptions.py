"""Prepare existing equipment for U3. Run in Unreal Python after rebuilding.

Default: validate a transient catalog, without saving. Pass --apply to back up
and save the catalog. --self-test runs without Unreal. Player saves are untouched.
"""
from datetime import datetime, timezone
import hashlib
from pathlib import Path
import shutil
import sys


# Initial tuning: retain each definition's existing base range, add small affixes.
# Power is an authored roll tier, not a stage or a replacement for attack damage.
TIERS = {15001: (1, 10), 15002: (11, 25), 15003: (11, 25),
         15004: (26, 40), 15005: (26, 40), 15006: (26, 40)}
NAMES = {'ATTACK': '공격력', 'HEALTH': '최대 생명력', 'DEFENSE': '방어력',
         'CRITICAL_RATE': '치명타 확률', 'CRITICAL_DAMAGE': '치명타 피해',
         'HEAL_AMOUNT': '회복량', 'MOVEMENT_SPEED': '이동 속도'}
# stat, weight, low-power min/max, high-power min/max, cap
POOLS = {
    'WEAPON': [('ATTACK', 3, 1, 3, 3, 5, 8),
               ('CRITICAL_RATE', 1, 25, 50, 50, 100, 150),
               ('CRITICAL_DAMAGE', 1, 1, 2, 2, 4, 6)],
    'ACCESSORY': [('HEALTH', 3, 5, 10, 10, 20, 30),
                  ('DEFENSE', 2, 1, 3, 3, 5, 8),
                  ('HEAL_AMOUNT', 1, 1, 2, 2, 4, 6)],
}


def self_test():
    assert set(TIERS) == set(range(15001, 15007))
    for low, high in TIERS.values():
        assert 1 <= low <= high <= 100000
    for rows in POOLS.values():
        assert len({r[0] for r in rows}) == len(rows) >= 2
        for stat, weight, low_min, low_max, high_min, high_max, cap in rows:
            assert stat in NAMES and weight > 0
            assert 0 <= low_min <= low_max <= cap <= 100000
            assert 0 <= high_min <= high_max <= cap
    print('PGEquipment option tuning SELF TEST PASS')


def run(apply=False):
    import unreal

    path = '/Game/DataCenter/Progression/DA_PGProgression'
    catalog = unreal.load_asset(path)
    assert catalog and hasattr(catalog, 'validate_equipment_generation'), \
        'U3 editor rebuild required; no assets changed'
    assert catalog.validate_equipment_generation() is not None, 'Invalid source catalog'
    # Copy structs so dry runs and validation failures cannot dirty the asset.
    original_items = list(catalog.get_editor_property('items'))
    items = [item.copy() for item in original_items]
    by_id = {item.get_editor_property('id'): item for item in items}
    assert len(by_id) == len(items) and set(TIERS) <= by_id.keys()

    def option(identifier, stat, slot, bounds, weight=1):
        result = unreal.PGItemOptionDefinition()
        values = dict(id=identifier, display_name=NAMES[stat], stat=getattr(unreal.PGStatType, stat),
                      allowed_slots=[slot], weight=float(weight),
                      min_at_low_power=bounds[0], max_at_low_power=bounds[1],
                      min_at_high_power=bounds[2], max_at_high_power=bounds[3], cap=bounds[4])
        for key, value in values.items():
            result.set_editor_property(key, value)
        return result

    for iid, (low, high) in TIERS.items():
        item = by_id[iid]
        assert not item.get_editor_property('use_option_pool'), (iid, 'Already converted; edit authored U3 data directly')
        slot = item.get_editor_property('slot')
        slot_name = next((name for name in POOLS if slot == getattr(unreal.PGEquipmentSlot, name)), None)
        assert slot_name, (iid, 'Unexpected slot; review tuning before migration')
        assert int(item.get_editor_property('rarity').value) == (0 if iid == 15001 else 1 if iid <= 15003 else 2)
        base = item.get_editor_property('base_options')
        bonus = item.get_editor_property('roll_bonus')
        assert base and 0 <= bonus <= 10000
        ranges = []
        for stat, value in base.items():
            name = next(name for name in NAMES if stat == getattr(unreal.PGStatType, name))
            assert 0 <= value <= value + bonus <= 100000
            ranges.append(option('Base.' + name, name, slot,
                                 (value, value + bonus, value, value + bonus, value + bonus)))
        affixes = [option('Affix.' + row[0], row[0], slot, row[2:], row[1]) for row in POOLS[slot_name]]
        for key, value in dict(use_option_pool=True, min_power=low, max_power=high,
                               drop_quality=unreal.PGItemQuality.STANDARD,
                               base_stat_ranges=ranges, implicit_options=[], option_pool=affixes).items():
            item.set_editor_property(key, value)

    # Native validation uses only these catalog fields. No live asset is edited yet.
    candidate = unreal.new_object(unreal.PGProgressionData)
    for key in ('rarity_rules', 'drop_pools', 'invalid_equipment_fallback_pool'):
        candidate.set_editor_property(key, catalog.get_editor_property(key))
    candidate.set_editor_property('items', items)
    assert candidate.validate_equipment_generation() is not None, 'Invalid candidate catalog'
    if not apply:
        unreal.log('PGEquipment U3 DRY RUN PASS definitions=6; no assets saved')
        return

    root = Path(unreal.Paths.project_dir()).resolve()
    source = root / 'Content/DataCenter/Progression/DA_PGProgression.uasset'
    backup = root / 'Saved/InventoryEquipment/OptionBackups' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup.mkdir(parents=True, exist_ok=False)
    destination = backup / source.name
    shutil.copy2(source, destination)
    assert hashlib.sha256(source.read_bytes()).digest() == hashlib.sha256(destination.read_bytes()).digest()
    try:
        catalog.set_editor_property('items', items)
        assert unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False), 'Catalog save failed'
    except Exception:
        catalog.set_editor_property('items', original_items)
        raise
    unreal.log('PGEquipment U3 APPLY PASS definitions=6 backup=' + str(destination))


if __name__ == '__main__':
    self_test()
    if '--self-test' not in sys.argv:
        run(apply='--apply' in sys.argv)
