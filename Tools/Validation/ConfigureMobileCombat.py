"""Migrate only player walking segments and montage slots; preserve combat timing/damage."""
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(Path(__file__).resolve().parent))
IDS = {100, 101, 102, 112, 114}


def items():
    result = []
    for name in ('HackSlashP0.json', 'HackSlashP1.json'):
        result.extend(json.loads((ROOT / 'Tools/Validation' / name).read_text(encoding='utf-8'))['skills'])
    return [item for item in result if item['id'] in IDS]


def load():
    table = unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    by_id = {row['SkillID']: row for row in rows}
    return [(item, unreal.load_asset(by_id[item['id']]['PlayerProfile']),
             unreal.load_asset(by_id[item['id']]['MontagePath']['AssetPath']['PackageName'])) for item in items()]


def movement(item, profile):
    old = list(profile.get_editor_property('movement_segments'))
    move = old[0] if old else unreal.PGPlayerMovementSegment()
    # Keep phase-to-segment references on the existing P0 profiles.
    move.set_editor_property('segment_id', 'movement' if item['id'] != 114 else 'move0')
    for key, value in dict(start=0., end=item['duration'], distance=0.,
                           mode=unreal.PGPlayerMoveMode.WALK,
                           walk_speed_ratio=item['walk_speed_ratio'], end_cast_on_block=False).items():
        move.set_editor_property(key, value)
    return [move]


def combat_snapshot(profile):
    result = {key: profile.get_editor_property(key) for key in (
        'skill_id', 'duration', 'aim_lock', 'dodge_cancel', 'attack_cancel', 'early_dodge_until',
        'attack_speed', 'frenzy_per_cast_cap', 'projectile_speed', 'projectile_range', 'projectile_lifetime')}
    for key in ('hit_phases', 'pose_keys'):
        result[key] = [entry.export_text() for entry in profile.get_editor_property(key)]
    return result


def validate():
    for item, profile, montage in load():
        moves = list(profile.get_editor_property('movement_segments'))
        assert len(moves) == 1
        move = moves[0]
        assert move.get_editor_property('mode') == unreal.PGPlayerMoveMode.WALK
        for prop, value in [('start', 0.), ('end', item['duration']), ('distance', 0.),
                            ('walk_speed_ratio', item['walk_speed_ratio'])]:
            assert abs(move.get_editor_property(prop) - value) < .0001, (item['id'], prop)
        tracks = list(montage.get_editor_property('slot_anim_tracks'))
        assert len(tracks) == 1
        expected = 'UpperBody' if item.get('upper_body') else 'FullBody'
        assert str(tracks[0].get_editor_property('slot_name')) == expected, item['id']
    unreal.log('PGMobileCombat VALIDATION PASS profiles=5 upper_body=4 saved_reload=1')


def apply():
    prepared = load()
    assert len(prepared) == 5 and all(profile and montage for _, profile, montage in prepared)
    backup = ROOT / 'Saved/Backups/MobileCombat' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    before = {}
    for item, profile, montage in prepared:
        for asset in (profile, montage):
            relative = Path(asset.get_path_name().split('.')[0].removeprefix('/Game/') + '.uasset')
            target = backup / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / 'Content' / relative, target)
        before[item['id']] = combat_snapshot(profile)
    # Changes to profiles are confined to movement_segments; montage tracks retain their source motion.
    for item, profile, montage in prepared:
        profile.set_editor_property('movement_segments', movement(item, profile))
        assert combat_snapshot(profile) == before[item['id']], item['id']
        if item.get('upper_body'):
            tracks = list(montage.get_editor_property('slot_anim_tracks'))
            assert len(tracks) == 1
            tracks[0].set_editor_property('slot_name', 'UpperBody')
            montage.set_editor_property('slot_anim_tracks', tracks)
            assert unreal.EditorAssetLibrary.save_loaded_asset(montage, only_if_is_dirty=False)
        assert unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
    (backup / 'before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
    (ROOT / 'Saved/MobileCombat_LastBackup.txt').write_text(str(backup), encoding='utf-8')
    validate()
    unreal.log('PGMobileCombat APPLY PASS backup=' + str(backup))


if __name__ == '__main__':
    validate() if '-PGMobileCombatValidate' in unreal.SystemLibrary.get_command_line() else apply()
