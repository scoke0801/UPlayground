"""Retune existing player profiles and montage crossfades without replacing gameplay data.

Run as an Unreal Python commandlet after compiling. All eight profiles and their
local player montages are backed up before saving. Tables and source sequences stay intact.
Pass -PGActiveSkillTempo to update only the five active profiles, preserving montages
and the already tuned basic combo. Times are absolute, so reapplying is idempotent.
"""
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import unreal

BLEND_SECONDS = .16


def configure_montage_blend(montage):
    for prop in ('blend_in', 'blend_out'):
        blend = montage.get_editor_property(prop)
        blend.set_editor_property('blend_time', BLEND_SECONDS)
        blend.set_editor_property('blend_option', unreal.AlphaBlendOption.CUBIC)
        montage.set_editor_property(prop, blend)


def timing(item, montage):
    p0 = 'pose' in item
    pose = item.get('pose') or item.get('pose_seconds')
    if not p0:
        pose = pose + [[item['duration'], montage.get_play_length()]] if pose else [
            [time, fraction * montage.get_play_length()] for time, fraction in item['pose_fraction']]
    moves = [[item['move_start'], item['move_end']]] if p0 else [move[:2] for move in item['moves']]
    width = item.get('hit_window', .06 if p0 or item['id'] == 113 else 0.)
    return pose, moves, width


def peak_pose_rate(pose):
    """Maximum Hermite derivative, including speed peaks between authored keys."""
    slopes = [(pb-pa)/(tb-ta) for (ta,pa),(tb,pb) in zip(pose,pose[1:])]
    tangents = [slopes[0]] + [2*a*b/(a+b) for a,b in zip(slopes,slopes[1:])] + [slopes[-1]]
    peak = 0.
    for i, slope in enumerate(slopes):
        left, right = tangents[i:i+2]
        a = -6*slope + 3*(left+right)
        b = 6*slope - 4*left - 2*right
        candidates = [0., 1.]
        if abs(a) > 1e-9 and 0 < -b/(2*a) < 1:
            candidates.append(-b/(2*a))
        peak = max(peak, *(a*x*x+b*x+left for x in candidates))
    return peak


def validate_profile_motion(item, profile, montage):
    pose, moves, width = timing(item, montage)
    for prop, expected in [('duration', item['duration']), ('attack_cancel', item['attack']),
                           ('dodge_cancel', item['dodge'])]:
        assert abs(profile.get_editor_property(prop) - expected) < .0001, (item['id'], prop)
    actual = profile.get_editor_property('pose_keys')
    assert len(actual) == len(pose)
    for key, (time, seconds) in zip(actual, pose):
        assert abs(key.get_editor_property('time') - time) < .0001
        assert abs(key.get_editor_property('montage_seconds') - seconds) < .0001
    for (ta, pa), (tb, pb) in zip(pose, pose[1:]):
        assert ta < tb and pa < pb and (pb-pa)/(tb-ta) <= 2.1, (item['id'], 'compressed pose segment')
    if 'max_pose_rate' in item:
        assert abs(profile.get_editor_property('attack_speed') - 1.) < .0001
        assert peak_pose_rate(pose) <= item['max_pose_rate'], (item['id'], 'compressed interpolated pose')
    if 'aim' in item:
        assert abs(profile.get_editor_property('aim_lock') - item['aim']) < .0001
    if 'early_dodge' in item:
        assert abs(profile.get_editor_property('early_dodge_until') - item['early_dodge']) < .0001
    actual = profile.get_editor_property('movement_segments')
    assert len(actual) == len(moves)
    for move, (start, end) in zip(actual, moves):
        assert abs(move.get_editor_property('start') - start) < .0001
        assert abs(move.get_editor_property('end') - end) < .0001
    phases = profile.get_editor_property('hit_phases')
    assert len(phases) == len(item['hits'])
    for phase, start in zip(phases, item['hits']):
        assert abs(phase.get_editor_property('start') - start) < .0001
        assert abs(phase.get_editor_property('end') - start - width) < .0001
    for prop in ('blend_in', 'blend_out'):
        blend = montage.get_editor_property(prop)
        assert abs(blend.get_editor_property('blend_time') - BLEND_SECONDS) < .0001
        assert blend.get_editor_property('blend_option') == unreal.AlphaBlendOption.CUBIC


def apply(active_only=False):
    root = Path(unreal.Paths.project_dir()).resolve()
    items = []
    for name in ('HackSlashP0.json', 'HackSlashP1.json'):
        items.extend(json.loads((root / 'Tools/Validation' / name).read_text(encoding='utf-8'))['skills'])
    if active_only:
        items = [item for item in items if item['id'] in (110,111,112,113,114)]
    table = unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    by_id = {r['SkillID']: r for r in rows}
    prepared = []
    for item in items:
        row = by_id[item['id']]
        profile = unreal.load_asset(row['PlayerProfile'])
        montage = unreal.load_asset(row['MontagePath']['AssetPath']['PackageName'])
        assert profile.get_editor_property('skill_id') == item['id']
        pose, moves, width = timing(item, montage)
        assert len(profile.get_editor_property('pose_keys')) == len(pose)
        assert len(profile.get_editor_property('movement_segments')) == len(moves)
        assert len(profile.get_editor_property('hit_phases')) == len(item['hits'])
        prepared.append((item, profile, montage))
    backup = root / 'Saved/Backups/AttackMotion' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    for _, profile, montage in prepared:
        for asset in ((profile,) if active_only else (profile, montage)):
            relative = asset.get_path_name().split('.')[0].removeprefix('/Game/') + '.uasset'
            target = backup / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists():
                shutil.copy2(root / 'Content' / relative, target)
    for item, profile, montage in prepared:
        pose, moves, width = timing(item, montage)
        for prop, value in [('duration',item['duration']), ('dodge_cancel',item['dodge']), ('attack_cancel',item['attack'])]:
            profile.set_editor_property(prop, value)
        if 'aim' in item:
            profile.set_editor_property('aim_lock', item['aim'])
        if 'early_dodge' in item:
            profile.set_editor_property('early_dodge_until', item['early_dodge'])
        for prop, values, fields in [('pose_keys',pose,('time','montage_seconds')),
                                     ('movement_segments',moves,('start','end')),
                                     ('hit_phases',[(t,t+width) for t in item['hits']],('start','end'))]:
            array = list(profile.get_editor_property(prop))
            for entry, pair in zip(array, values):
                for field, value in zip(fields, pair):
                    entry.set_editor_property(field, value)
            profile.set_editor_property(prop, array)
        if not active_only:
            configure_montage_blend(montage)
        validate_profile_motion(item, profile, montage)
        for asset in ((profile,) if active_only else (profile, montage)):
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.log(f'PGAttackMotion APPLY PASS profiles={len(items)} active_only={active_only} backup={backup}')


if __name__ == '__main__':
    apply(active_only='-PGActiveSkillTempo' in unreal.SystemLibrary.get_command_line())
