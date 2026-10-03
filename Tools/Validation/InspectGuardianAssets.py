"""Read the current guardian rig and compatible animation assets; save no UE assets."""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'Guardian'
out.mkdir(parents=True, exist_ok=True)
def rows(path):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))
enemy = next(r for r in rows('/Game/DataCenter/DataTables/Actor/DT_Enemy') if r['EnemyID'] == 15103)
skill = next(r for r in rows('/Game/DataCenter/DataTables/Skill/DT_Skill') if r['SkillID'] == 15103)
cdo = unreal.get_default_object(unreal.load_class(None, enemy['ActorClass']))
mesh = cdo.get_editor_property('mesh')
asset = mesh.get_editor_property('skeletal_mesh_asset')
skeleton = asset.get_editor_property('skeleton')
report = dict(enemy=enemy, skill=skill, mesh=asset.get_path_name(), skeleton=skeleton.get_path_name(),
              mesh_transform=str(mesh.get_relative_transform()), anim_class=str(mesh.get_editor_property('anim_class')),
              bones=[str(mesh.get_bone_name(i)) for i in range(mesh.get_num_bones())],
              properties={k:str(cdo.get_editor_property(k)) for k in ['left_hand_collision_box_attach_bone_name','right_hand_collision_box_attach_bone_name']}, animations=[])
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
for entry in registry.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', 'AnimSequence'), True):
    tag = str(entry.get_tag_value('Skeleton') or '')
    if skeleton.get_path_name() not in tag:
        continue
    anim = entry.get_asset()
    report['animations'].append(dict(path=anim.get_path_name(), length=anim.get_play_length()))
report['api'] = {name:[x for x in dir(getattr(unreal,name)) if any(t in x for t in ['pose','bone','track','controller','montage','section'])]
                 for name in ['AnimationLibrary','AnimSequence','AnimMontage','AnimDataController','AnimPoseExtensions'] if hasattr(unreal,name)}
(out/'inspection.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('PGGuardian inspection complete')
