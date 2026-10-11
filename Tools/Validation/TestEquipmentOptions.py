"""Offline safety checks; these do not substitute for Unreal reflection validation."""
import copy
from enum import IntEnum
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import ConfigureEquipmentOptions as migration


class Struct:
    def __init__(self, **values):
        self.values = values

    def get_editor_property(self, key):
        return self.values[key]

    def set_editor_property(self, key, value):
        self.values[key] = value

    def copy(self):
        return copy.deepcopy(self)


class Catalog(Struct):
    def validate_equipment_generation(self):
        return ''


class MigrationTest(unittest.TestCase):
    def setUp(self):
        stats = IntEnum('Stat', list(migration.NAMES))
        slots = IntEnum('Slot', ['WEAPON', 'ACCESSORY'])
        quality = IntEnum('Quality', ['STANDARD', 'ANCESTRAL'])
        rarities = IntEnum('Rarity', {'COMMON': 0, 'MAGIC': 1, 'RARE': 2})
        items = []
        for iid in migration.TIERS:
            items.append(Struct(id=iid, use_option_pool=False,
                                slot=slots.ACCESSORY if iid in (15003, 15005) else slots.WEAPON,
                                rarity=rarities(0 if iid == 15001 else 1 if iid <= 15003 else 2),
                                base_options={stats.ATTACK: 25}, roll_bonus=10,
                                icon='preserved', combat_perks={'existing': 12}))
        self.catalog = Catalog(items=items, rarity_rules=['preserved'], drop_pools=['preserved'],
                               invalid_equipment_fallback_pool='None')
        self.candidate = Catalog()
        self.logs = []
        self.unreal = SimpleNamespace(load_asset=lambda _: self.catalog,
                                      PGStatType=stats, PGEquipmentSlot=slots, PGItemQuality=quality,
                                      PGItemOptionDefinition=Struct, PGProgressionData=Catalog,
                                      new_object=lambda _: self.candidate, log=self.logs.append)

    def test_dry_run_preserves_catalog_and_existing_fields(self):
        before = copy.deepcopy(self.catalog.values)
        with patch.dict(sys.modules, unreal=self.unreal):
            migration.run()
        for old, current, converted in zip(before['items'], self.catalog.values['items'], self.candidate.values['items']):
            self.assertEqual(old.values, current.values)
            for key in ('icon', 'combat_perks', 'base_options', 'roll_bonus', 'rarity', 'slot'):
                self.assertEqual(old.values[key], converted.values[key])
            self.assertTrue(converted.values['use_option_pool'])
            self.assertEqual(len(converted.values['option_pool']), 3)
            base = converted.values['base_stat_ranges'][0].values
            self.assertEqual((base['min_at_low_power'], base['max_at_high_power']), (25, 35))
        self.assertIn('DRY RUN PASS', self.logs[0])

    def test_old_binary_rejected_before_mutation(self):
        self.catalog = Struct(items=[])
        with patch.dict(sys.modules, unreal=self.unreal), self.assertRaisesRegex(AssertionError, 'rebuild required'):
            migration.run()

    def test_converted_definition_not_overwritten(self):
        self.catalog.values['items'][0].values['use_option_pool'] = True
        with patch.dict(sys.modules, unreal=self.unreal), self.assertRaisesRegex(AssertionError, 'Already converted'):
            migration.run()
        self.assertEqual(self.candidate.values, {})

    def test_native_failure_does_not_modify_catalog(self):
        self.candidate.validate_equipment_generation = lambda: None
        with patch.dict(sys.modules, unreal=self.unreal), self.assertRaisesRegex(AssertionError, 'Invalid candidate'):
            migration.run(apply=True)
        self.assertFalse(any(i.values['use_option_pool'] for i in self.catalog.values['items']))


if __name__ == '__main__':
    unittest.main()
