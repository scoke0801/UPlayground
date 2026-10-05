"""Read the actual skeletons and gameplay templates before authoring retarget data."""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PlayableCharacters'
OUT.mkdir(parents=True, exist_ok=True)
paths = {n: '/Game/Art/ToonTest/'+n+'/SK_'+n+'_ToonTest' for n in ('Bokusei','LianLian','Honoka')}
paths.update({n: '/Game/Art/ToonCharacters/'+n+'/SK_PG_'+n for n in ('Hichi','Suiha','lili','Nenmir')})
paths.update({n: '/Game/Art/PlayerModels/'+n+'/SK_PG_'+n for n in ('Hwarin','Arin','Yura')})
p09 = json.loads((ROOT/'Saved/P09Modular/configure.json').read_text(encoding='utf-8'))
for sex in ('Female','Male'):
    paths['P09'+sex] = next(r['asset'] for r in p09['meshes'] if r['name']==sex+'_Face_01')
paths['PlayerSource']='/Game/ExternalAssets/Characters/ElfSelena/BaseMesh/SK_ElfSelena'
enemy_table=unreal.load_asset('/Game/DataCenter/DataTables/Actor/DT_Enemy')
enemy_rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(enemy_table))
enemy_row=next(r for r in enemy_rows if r['EnemyID']==15101)
enemy_bp=unreal.load_asset(enemy_row['ActorClass'].split('.')[0])
enemy_cdo=unreal.get_default_object(enemy_bp.generated_class())
paths['EnemySource']=enemy_cdo.mesh.get_skeletal_mesh_asset().get_path_name()
result={}
for name,path in paths.items():
    mesh=unreal.load_asset(path)
    if not mesh:
        result[name]={'missing':path}
        continue
    comp=unreal.SkeletalMeshComponent()
    comp.set_skeletal_mesh_asset(mesh)
    rig=unreal.IKRigDefinition()
    ctl=unreal.IKRigController.get_controller(rig)
    ctl.set_skeletal_mesh(mesh)
    bones=[]
    for i in range(comp.get_num_bones()):
        bone=comp.get_bone_name(i)
        t=ctl.get_ref_pose_transform_of_bone(bone)
        bones.append(dict(name=str(bone),parent=str(comp.get_parent_bone(bone)),position=[t.translation.x,t.translation.y,t.translation.z],scale=[t.scale3d.x,t.scale3d.y,t.scale3d.z]))
    result[name]=dict(mesh=path,skeleton=mesh.skeleton.get_path_name(),bones=bones)
result['blueprints']=[str(a.package_name) for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/Blueprints/Actor',recursive=True) if str(a.asset_class_path.asset_name)=='Blueprint']
result['enemy_template']=enemy_row
result['enemy_transform']=str(enemy_cdo.mesh.get_relative_transform())
player_cdo=unreal.get_default_object(unreal.load_asset('/Game/Blueprints/Actor/LocalPlayer/BP_LocalPlayer').generated_class())
result['player_transform']=str(player_cdo.mesh.get_relative_transform())
result['stat_tables']=[str(a.package_name) for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/DataCenter',recursive=True) if 'Stat' in str(a.asset_name)]
if hasattr(unreal,'PGCharacterAppearance'):
    asset=unreal.load_asset('/Game/DataCenter/Characters/DA_Bokusei')
    if asset:
        path=asset.get_editor_property('retargeter')
        result['soft_path_api']=dict(value=str(path),members=dir(path),system=[n for n in dir(unreal.SystemLibrary) if 'soft' in n.lower()])
(OUT/'inspection.json').write_text(json.dumps(result,indent=2),encoding='utf-8')

