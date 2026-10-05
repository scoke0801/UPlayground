"""Back up current content, create opt-in P0 profiles and update exactly five skill rows.

Run in Unreal Python after compiling native types. No profile saves, loadout data,
enemy patterns, source sequences or legacy PlayerAttacks.json are overwritten.
Player montage blend settings follow the shared attack-motion tuning.
"""
from datetime import datetime, timezone
import copy
import json
from pathlib import Path
import shutil
import sys
import unreal
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureAttackMotion import configure_montage_blend

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/DataCenter/HackSlashP0'
SPEC = json.loads((ROOT / 'Tools/Validation/HackSlashP0.json').read_text(encoding='utf-8'))
BACKUP = ROOT / 'Saved/Backups/HackSlashP0' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
BACKUP.mkdir(parents=True)
tools = unreal.AssetToolsHelpers.get_asset_tools()


def preserve(asset):
    relative = asset.get_path_name().split('.')[0].removeprefix('/Game/') + '.uasset'
    source = ROOT / 'Content' / relative
    target = BACKUP / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    if source.exists() and not target.exists():
        shutil.copy2(source, target)


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def set_props(obj, values):
    for key, value in values.items():
        obj.set_editor_property(key, value)
    return obj


table = unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
before = copy.deepcopy(rows)
by_id = {r['SkillID']: r for r in rows}
for item in SPEC['skills']:
    assert 'PlayerProfile' in by_id[item['id']], 'Compile native profile support first'
    montage = unreal.load_asset(by_id[item['id']]['MontagePath']['AssetPath']['PackageName'])
    assert montage and item['pose'][-1][1] <= montage.get_play_length()
preserve(table)
(BACKUP / 'skills.json').write_text(json.dumps(before, ensure_ascii=False, indent=2), encoding='utf-8')

# Reusable lightweight translucent arc. One component is reused per player; no collision.
material = unreal.load_asset(DEST + '/M_PlayerSlash')
if material:
    preserve(material)
else:
    material = tools.create_asset('M_PlayerSlash', DEST, unreal.Material, unreal.MaterialFactoryNew())
from PlayerSlashMaterial import build as build_slash_material
build_slash_material(material)
save(material)

sound = unreal.load_asset('/Game/DataCenter/CombatCycle/S_Normal')
mapping = []
for item in SPEC['skills']:
    skill_id = item['id']
    name = 'DA_PlayerSkill_' + str(skill_id)
    profile = unreal.load_asset(DEST + '/' + name)
    if profile:
        preserve(profile)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.PGPlayerSkillProfile)
        profile = tools.create_asset(name, DEST, unreal.PGPlayerSkillProfile, factory)
    movement = set_props(unreal.PGPlayerMovementSegment(), {
        'segment_id':'movement', 'start':item['move_start'], 'end':item['move_end'],
        'mode':unreal.PGPlayerMoveMode.WALK if skill_id == 112 else unreal.PGPlayerMoveMode.FORWARD_SWEEP,
        'distance':item['distance'], 'walk_speed_ratio':.6,
        'end_cast_on_block':skill_id == 111})
    phases = []
    for index, start in enumerate(item['hits']):
        policy = set_props(unreal.PGHitProcPolicy(), {'bleed':True, 'bleed_burst':skill_id in (111,112) and index == 1, 'shock':True, 'frenzy':True})
        phases.append(set_props(unreal.PGPlayerHitPhase(), {
            'phase_id':index, 'start':start, 'end':start+item.get('hit_window', .06),
            'shape':unreal.PGPlayerHitShape.DISC if skill_id == 112 else unreal.PGPlayerHitShape.FAN,
            'radius':item['radius'], 'full_angle_degrees':item['angle'], 'height_tolerance':150,
            'wall_occlusion':True, 'damage_multiplier':item['damage'], 'heavy_impact':skill_id == 102,
            'hit_stop_seconds':.045 if skill_id == 102 else .020 if skill_id == 112 else .025,
            'movement_segment_id':'movement', 'proc_policy':policy}))
    keys = [set_props(unreal.PGPlayerPoseKey(), {'time':t, 'montage_seconds':m}) for t,m in item['pose']]
    set_props(profile, {'skill_id':skill_id, 'duration':item['duration'], 'aim_lock':item.get('aim', .1),
                        'dodge_cancel':item['dodge'], 'attack_cancel':item['attack'], 'attack_speed':1.,
                        'frenzy_per_cast_cap':3, 'hit_phases':phases, 'movement_segments':[movement],
                        'pose_keys':keys, 'slash_material':material, 'swing_sound':sound})
    save(profile)
    row = by_id[skill_id]
    row['PlayerProfile'] = profile.get_path_name()
    row['SkillCoolTime'] = item['cooldown']
    row['Desc'] = item['name']
    montage = unreal.load_asset(row['MontagePath']['AssetPath']['PackageName'])
    preserve(montage)
    configure_montage_blend(montage)
    save(montage)
    mapping.append({'skill':skill_id, 'montage':montage.get_path_name(), 'length':montage.get_play_length(),
                    'source_rate_scale':montage.get_editor_property('rate_scale'), 'pose_keys':item['pose'],
                    'logical_rate':1.0, 'legacy_rate_not_applied':row['PlayerAttackPlayRate']})
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False))
save(table)
current = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
allowed = {item['id'] for item in SPEC['skills']}
for old, new in zip(before, current):
    if old['SkillID'] not in allowed:
        assert old == new, old['SkillID']
    else:
        for key in old.keys() - {'PlayerProfile','SkillCoolTime','Desc'}:
            assert old[key] == new[key], (old['SkillID'],key)
(BACKUP / 'mapping.json').write_text(json.dumps(mapping, ensure_ascii=False, indent=2), encoding='utf-8')
(ROOT / 'Saved/HackSlashP0_LastBackup.txt').write_text(str(BACKUP), encoding='utf-8')
unreal.log('PGHackSlash APPLY PASS profiles=5 unchanged_other_rows=' + str(len(rows)-5) + ' backup=' + str(BACKUP))
