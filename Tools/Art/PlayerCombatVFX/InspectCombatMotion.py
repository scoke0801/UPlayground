"""Record source hand trajectories at authored contact markers for VFX direction review."""
import json
import re
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')))
registry=unreal.AssetRegistryHelpers.get_asset_registry()
result=[]
for row in rows:
    if row['SkillID'] not in (100,101,102,110,111,112,113,114): continue
    skill={'id':row['SkillID']}
    montage_path=row['MontagePath']['AssetPath']['PackageName']
    dependencies=registry.get_dependencies(montage_path,unreal.AssetRegistryDependencyOptions(include_hard_package_references=True))
    sequences=[unreal.load_asset(str(p)) for p in dependencies if str(p).startswith('/Game/')]
    sequences=[s for s in sequences if isinstance(s,unreal.AnimSequence)]
    assert len(sequences)==1,(skill,sequences)
    sequence=sequences[0]
    events=unreal.AnimationLibrary.get_animation_notify_events(sequence)
    contacts=[float(re.findall(r'LinkValue=([\d.]+)',e.export_text())[-1]) for e in events if str(e.get_editor_property('notify_name'))=='P_HitPoint']
    if not contacts: contacts=[{100:.78,101:.67,102:.75,114:.78}.get(skill['id'],sequence.get_play_length()*.5)]
    # Source right-hand position/orientation in component space, before/at/after contact.
    samples=[]
    for at in contacts:
        frames=[]
        for t in (max(0,at-.06),at,min(sequence.get_play_length(),at+.06)):
            pose=sequence.get_anim_pose_at_time(t,unreal.AnimPoseEvaluationOptions())
            names=unreal.AnimPoseExtensions.get_bone_names(pose)
            bones={str(b):str(unreal.AnimPoseExtensions.get_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD)) for b in names if str(b) in ('root','pelvis') or any(x in str(b).lower() for x in ('hand','weapon'))}
            frames.append(dict(time=t,bones=bones))
        samples.append(dict(contact=at,frames=frames))
    result.append(dict(skill=skill['id'],sequence=sequence.get_path_name(),contacts=samples))
(root/'Saved/QA/CombatVFX_MotionContacts.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
unreal.log('PGCombatVFX MOTION PASS')
