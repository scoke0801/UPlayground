"""Read persisted representative clips, contacts, retargeting and encounter data."""
import json
import os
from pathlib import Path
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT / 'Tools/Validation'))
from PlayableCharacterPolish import snapshot

out = Path(os.environ.get('PG_FOCUSED_INSPECTION_DIR', str(ROOT / 'Saved/QA/FocusedCombatInspection')))
out.mkdir(parents=True, exist_ok=True)
def rows(path):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))

result = dict(skills=[], enemy=[], stats=[], appearance={})
for row in rows('/Game/DataCenter/DataTables/Skill/DT_Skill'):
    if row['SkillID'] not in (100, 101, 102, 112, 114, 10000, 15751): continue
    path = row['ElitePresentationMontage'] if row['SkillID'] == 15751 else row['MontagePath']['AssetPath']['PackageName']
    montage = unreal.load_asset(path)
    record = dict(row=row, montage=path, clips=[])
    for track in montage.get_editor_property('slot_anim_tracks'):
        for segment in track.get_editor_property('anim_track').get_editor_property('anim_segments'):
            clip = segment.get_editor_property('anim_reference')
            record['clips'].append(dict(path=clip.get_path_name(), slot=str(track.get_editor_property('slot_name')),
                length=clip.get_play_length(), root_motion=clip.get_editor_property('enable_root_motion'),
                root_lock=clip.get_editor_property('force_root_lock'),
                segment=segment.export_text(), notifies=[e.export_text() for e in unreal.AnimationLibrary.get_animation_notify_events(clip)]))
            options = unreal.AnimPoseEvaluationOptions()
            options.extract_root_motion = False
            trajectory = []
            for i in range(61):
                at = clip.get_play_length() * i / 60
                pose = clip.get_anim_pose_at_time(at, options)
                bones = {}
                for bone in ('root', 'pelvis', 'foot_l', 'foot_r', 'hand_r'):
                    transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
                    v, q = transform.translation, transform.rotation
                    bones[bone] = dict(position=[v.x,v.y,v.z], rotation=[q.x,q.y,q.z,q.w])
                trajectory.append(dict(time=at, bones=bones))
            record['clips'][-1]['source_trajectory'] = trajectory
    if row['SkillID'] in (100, 101, 102, 112, 114):
        profile = unreal.load_asset(row['PlayerProfile'])
        record['profile'] = {p: str(profile.get_editor_property(p)) for p in ('duration', 'attack_cancel', 'dodge_cancel', 'early_dodge_until', 'swing_notify_name')}
        record['profile'].update({p: [e.export_text() for e in profile.get_editor_property(p)] for p in ('hit_phases', 'pose_keys', 'movement_segments')})
    result['skills'].append(record)
result['enemy'] = [r for r in rows('/Game/DataCenter/DataTables/Actor/DT_Enemy') if r['EnemyID'] == 15701]
result['stats'] = rows('/Game/DataCenter/DataTables/Actor/DT_CharacterStat')
for name in ('DA_Bokusei', 'RTG_Bokusei', 'IK_Bokusei_Source', 'IK_Bokusei_Target'):
    asset = unreal.load_asset('/Game/DataCenter/Characters/' + name)
    assert asset, name
    result['appearance'][name] = snapshot(asset)
(out / 'inspection.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('PGFocusedCombat INSPECT PASS')
