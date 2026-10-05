"""Save only changed appearance grips using the existing package transaction."""
import copy
import os
from pathlib import Path
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from PlayableCharacterPolish import SOURCE, STAMP, read_source, preflight, snapshot, equivalent, digest, struct_value, validate_appearance, path
from PlayableCharacterTransaction import Transaction, write_json

output = Path(os.environ['PG_CHARACTER_RUN'])
source = read_source()
preflight(source)
changes = []
portraits = {}
for package, wanted in source['assets'].items():
    asset = unreal.load_asset(package)
    actual = snapshot(asset)
    if equivalent(actual, wanted):
        continue
    if wanted['kind'] != 'appearance':
        raise ValueError('Apply/export pending non-grip changes first: '+package)
    without_grip = copy.deepcopy(actual)
    without_grip['fields']['grip_profiles'] = wanted['fields']['grip_profiles']
    if not equivalent(without_grip, wanted):
        raise ValueError('Apply/export pending appearance changes first: '+package)
    profiles = [struct_value(unreal.PGAppearanceGripProfile, text) for text in wanted['fields']['grip_profiles']]
    original = list(asset.get_editor_property('grip_profiles'))
    try:
        asset.set_editor_property('grip_profiles', profiles)
        validate_appearance(asset)
    finally:
        asset.set_editor_property('grip_profiles', original)
    changes.append((package, asset, profiles))
    portraits[package] = path(asset.get_editor_property('portrait'))

transaction = Transaction(ROOT, output)
transaction.prepare([package for package, _, _ in changes], SOURCE)
for package, asset, profiles in changes:
    asset.set_editor_property('grip_profiles', profiles)
    actual = snapshot(asset)
    assert equivalent(actual, source['assets'][package]), package
    unreal.EditorAssetLibrary.set_metadata_tag(asset, STAMP, digest(actual))
    transaction.mark_written(package)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), package
write_json(output/'configure.json', dict(status='SAVED', saved_count=len(changes),
    changed_packages=[package for package, _, _ in changes], preserved_portraits=portraits))
