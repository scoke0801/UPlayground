"""Run in a fresh Unreal process after applying ConfigureEquipmentOptions.py."""
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(unreal.Paths.project_dir()) / 'Tools/Validation'))
from ConfigureEquipmentOptions import TIERS, POOLS

catalog = unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
assert catalog
assert catalog.validate_equipment_generation() is not None, 'Native equipment generation validation failed'
items = {item.get_editor_property('id'): item for item in catalog.get_editor_property('items')}
for iid, (low, high) in TIERS.items():
    item = items[iid]
    assert item.get_editor_property('use_option_pool'), iid
    assert (item.get_editor_property('min_power'), item.get_editor_property('max_power')) == (low, high), iid
    assert item.get_editor_property('drop_quality') == unreal.PGItemQuality.STANDARD, iid
    slot = item.get_editor_property('slot')
    slot_name = next(name for name in POOLS if slot == getattr(unreal.PGEquipmentSlot, name))
    options = list(item.get_editor_property('option_pool'))
    assert len(options) == len(POOLS[slot_name]), iid
    for option, row in zip(options, POOLS[slot_name]):
        assert str(option.get_editor_property('id')) == 'Affix.' + row[0], iid
        assert option.get_editor_property('stat') == getattr(unreal.PGStatType, row[0]), iid
        assert option.get_editor_property('weight') == row[1], iid
        assert list(option.get_editor_property('allowed_slots')) == [slot], iid
        for key, value in zip(('min_at_low_power', 'max_at_low_power', 'min_at_high_power', 'max_at_high_power', 'cap'), row[2:]):
            assert option.get_editor_property(key) == value, (iid, key)
    base = item.get_editor_property('base_options')
    ranges = list(item.get_editor_property('base_stat_ranges'))
    assert len(ranges) == len(base), iid
    for option in ranges:
        value = base[option.get_editor_property('stat')]
        maximum = value + item.get_editor_property('roll_bonus')
        assert option.get_editor_property('min_at_low_power') == value, iid
        assert option.get_editor_property('min_at_high_power') == value, iid
        assert option.get_editor_property('max_at_low_power') == maximum, iid
        assert option.get_editor_property('max_at_high_power') == maximum, iid
unreal.log('PGEquipment U3 RELOAD PASS definitions=6')
