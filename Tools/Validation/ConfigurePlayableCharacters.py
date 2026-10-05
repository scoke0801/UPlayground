"""Author the seven selectable toon identities and four extensible P09 enemy templates.

Run using UE 5.8 Python. Backs up every existing package before saving; source art,
source animations and arena wave compositions are preserved.
"""
import copy
import json
import shutil
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/PlayableCharacters'
RUN=OUT/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
RUN.mkdir(parents=True)
DEST='/Game/DataCenter/Characters'
EAL=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
REPORT=dict(status='RUNNING',run=str(RUN),players=[],enemies=[])
preserved=set()

def preserve(path):
    package=path.split('.')[0]
    if package in preserved: return
    preserved.add(package)
    relative=Path(package.removeprefix('/Game/')+'.uasset')
    source=ROOT/'Content'/relative
    if source.exists():
        target=RUN/'backup'/relative
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)

def save(asset):
    preserve(asset.get_path_name())
    assert EAL.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()

def own(name,cls,factory):
    path=DEST+'/'+name
    preserve(path)
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(name,DEST,cls,factory)

def data(name):
    factory=unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class',unreal.PGCharacterAppearance)
    return own(name,unreal.PGCharacterAppearance,factory)

def skeleton(mesh):
    comp=unreal.SkeletalMeshComponent()
    comp.set_skeletal_mesh_asset(mesh)
    names=[str(comp.get_bone_name(i)) for i in range(comp.get_num_bones())]
    parents={n:str(comp.get_parent_bone(n)) for n in names}
    by_lower={n.lower():n for n in names}
    def find(*options):
        return next((by_lower[n.lower()] for n in options if n.lower() in by_lower),None)
    pelvis=find('pelvis','Hips','Hip')
    head=find('Head'); neck=find('neck_01','Neck')
    spine=find('spine_01','Spine'); chest=find('spine_05','spine_03','UpperChest','Chest')
    assert all((pelvis,head,neck,spine,chest)),mesh.get_path_name()
    chains=[('Spine',spine,chest),('Neck',neck,parents[head]),('Head',head,head)]
    if parents[head]==chest: chains[1]=('Neck',neck,neck)
    for side in ('l','r'):
        hand=find('hand_'+side); foot=find('foot_'+side)
        upper=parents[parents[hand]]; thigh=parents[parents[foot]]
        shoulder=parents[upper]; toe=find('ball_'+side,'Toes_'+side,'Toe_'+side)
        chains += [('Clavicle_'+side,shoulder,shoulder),('Arm_'+side,upper,hand),('Leg_'+side,thigh,foot)]
        if toe: chains.append(('Toe_'+side,toe,toe))
        # Infer fingers by their actual parent chain; do not include twist/helper bones.
        for finger,alias in [('thumb','Thumb'),('index','Index'),('middle','Middle'),('ring','Ring'),('pinky','Little')]:
            first=find(finger+'_01_'+side,alias+'Proximal_'+side,alias+'_Proximal_'+side,alias+'1_'+side,alias+'_1_'+side,finger+'1_'+side,
                       'finger_'+('thumbs' if finger=='thumb' else alias.lower())+'_proximal_'+side)
            if first:
                children=[n for n in names if parents[n]==first and 'end' not in n.lower()]
                second=children[0] if children else None
                children=[n for n in names if parents[n]==second and 'end' not in n.lower()]
                if children: chains.append((finger+'_'+side,first,children[0]))
    return pelvis,head,chains

def rig(name,mesh):
    asset=own(name,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
    ctl=unreal.IKRigController.get_controller(asset)
    assert ctl.set_skeletal_mesh(mesh)
    pelvis,head,chains=skeleton(mesh)
    for chain in list(ctl.get_retarget_chains()): ctl.remove_retarget_chain(chain.chain_name)
    assert ctl.set_retarget_root(pelvis)
    for chain,start,end in chains: assert str(ctl.add_retarget_chain(chain,start,end,'None'))==chain
    save(asset)
    return asset,ctl,head,chains

def retarget(name,source,target):
    sr,_,_,src_chains=rig('IK_'+name+'_Source',source)
    tr,tctl,head,dst_chains=rig('IK_'+name+'_Target',target)
    asset=own('RTG_'+name,unreal.IKRetargeter,unreal.IKRetargetFactory())
    ctl=unreal.IKRetargeterController.get_controller(asset)
    s,t=unreal.RetargetSourceOrTarget.SOURCE,unreal.RetargetSourceOrTarget.TARGET
    ctl.remove_all_ops()
    ctl.set_ik_rig(s,sr); ctl.set_ik_rig(t,tr)
    ctl.set_preview_mesh(s,source); ctl.set_preview_mesh(t,target)
    for op in ('IKRetargetPelvisMotionOp','IKRetargetFKChainsOp'):
        index=ctl.add_retarget_op('/Script/IKRig.'+op)
        assert index>=0
        ctl.run_op_initial_setup(index)
    ctl.assign_ik_rig_to_all_ops(s,sr); ctl.assign_ik_rig_to_all_ops(t,tr)
    source_names={c[0] for c in src_chains}
    for chain,_,_ in dst_chains:
        if chain in source_names: assert ctl.set_source_chain(chain,chain)
    ctl.reset_retarget_pose('Default Pose',[],t)
    ctl.auto_align_all_bones(t,unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    save(asset)
    return asset,tctl,head

def appearance(identity,label,source,target,parts=()):
    rtg,ctl,head=retarget(identity,source,target)
    asset=data('DA_'+identity)
    asset.set_editor_property('id',identity)
    asset.set_editor_property('display_name',label)
    asset.set_editor_property('mesh',target)
    asset.set_editor_property('source_mesh',source)
    asset.set_editor_property('retargeter',unreal.SoftObjectPath(rtg.get_path_name()))
    asset.set_editor_property('reconstruct_scaled_translations',identity.startswith('P09_'))
    asset.set_editor_property('head_bone',head)
    pose=ctl.get_ref_pose_transform_of_bone(head)
    asset.set_editor_property('head_forward_axis',unreal.MathLibrary.inverse_transform_direction(pose,unreal.Vector(0,1,0)))
    asset.set_editor_property('head_right_axis',unreal.MathLibrary.inverse_transform_direction(pose,unreal.Vector(1,0,0)))
    # Native target proportions stay intact. Honoka's imported origin has a 23.5 cm lift.
    asset.set_editor_property('mesh_transform',unreal.Transform(location=unreal.Vector(0,0,-23.5 if identity=='Honoka' else 0)))
    entries=[]
    for mesh in parts:
        row=unreal.PGAppearancePart()
        row.set_editor_property('mesh',mesh)
        if mesh.skeleton!=target.skeleton:
            row.set_editor_property('attach_bone',head)
            # Independent hair is authored in full-body reference coordinates.
            row.set_editor_property('relative_transform',unreal.MathLibrary.invert_transform(pose))
        entries.append(row)
    asset.set_editor_property('parts',entries)
    _,_,source_chains=skeleton(source)
    _,_,target_chains=skeleton(target)
    targets={n:(start,end) for n,start,end in target_chains}
    equipment={}
    for chain,start,end in source_chains:
        if chain in targets:
            equipment[start]=targets[chain][0]
            equipment[end]=targets[chain][1]
    asset.set_editor_property('equipment_bones',equipment)
    save(asset)
    return asset

def table_rows(path):
    table=unreal.load_asset(path)
    return table,json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))

def main():
    source=unreal.load_asset('/Game/ExternalAssets/Characters/ElfSelena/BaseMesh/SK_ElfSelena')
    players=[]
    for identity,art in [('Bokusei','Bokusei'),('LianLian','LianLian'),('Honoka','Honoka'),('Hichi','Hichi'),('Siuha','Suiha'),('Lili','lili'),('Nenmir','Nenmir')]:
        meshpath=('/Game/Art/ToonTest/'+art+'/SK_'+art+'_ToonTest') if art in ('Bokusei','LianLian','Honoka') else '/Game/Art/ToonCharacters/'+art+'/SK_PG_'+art
        target=unreal.load_asset(meshpath)
        assert target,meshpath
        asset=appearance(identity,identity,source,target)
        players.append(asset)
        REPORT['players'].append(dict(id=identity,asset=asset.get_path_name(),mesh=meshpath))
    catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
    preserve(catalog.get_path_name())
    catalog.set_editor_property('playable_characters',players)
    save(catalog)
    player_bp=unreal.load_asset('/Game/Blueprints/Actor/LocalPlayer/BP_LocalPlayer')
    preserve(player_bp.get_path_name())
    camera=unreal.get_default_object(player_bp.generated_class()).get_component_by_class(unreal.CameraComponent)
    assert camera
    camera.add_or_update_blendable(unreal.load_asset('/Game/Art/ToonTest/Advanced/Materials/M_PGToonScreenOutline'),1.)
    save(player_bp)
    enemies,rows=table_rows('/Game/DataCenter/DataTables/Actor/DT_Enemy')
    for row in rows:
        if row['EnemyID'] in range(15201,15205):
            assert row['ActorClass'].startswith(DEST+'/BP_PGEnemy_P09_'),'Enemy ID already belongs to another asset: '+str(row['EnemyID'])
    death_table,death_rows=table_rows('/Game/DataCenter/DataTables/Path/DT_Death')
    death_template=next((r for r in death_rows if r['ObjectTID']==15101),None)
    template=next(r for r in rows if r['EnemyID']==15101)
    parent=unreal.load_asset(template['ActorClass'].split('.')[0])
    source=unreal.get_default_object(parent.generated_class()).mesh.get_skeletal_mesh_asset()
    p09=json.loads((ROOT/'Tools/Art/P09Modular/manifest.json').read_text(encoding='utf-8'))
    audit=json.loads((ROOT/'Saved/P09Modular/configure.json').read_text(encoding='utf-8'))
    mesh_paths={r['name']:r['asset'] for r in audit['meshes']}
    stat_assets=[a for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/DataCenter/DataTables',recursive=True) if str(a.asset_class_path.asset_name)=='DataTable']
    stat_table=stat_rows=None
    for entry in stat_assets:
        candidate,candidate_rows=table_rows(str(entry.package_name))
        if candidate_rows and 'CharacterID' in candidate_rows[0] and any(r['CharacterID']==15101 for r in candidate_rows):
            stat_table,stat_rows=candidate,candidate_rows
            break
    assert stat_table,'Missing enemy stat table'
    stat_template=next(r for r in stat_rows if r['CharacterID']==15101)
    for index,label in enumerate(('Female','Male','Female_Armor007','Male_Armor007')):
        sex=label.split('_')[0]
        names=p09['presets'][sex]['parts'] if '_' not in label else [sex+'_Face_01','Hair_09']+[r['name'] for r in p09['parts'] if r['name'].startswith(('Fem' if sex=='Female' else 'Male')+'_Armor_007_')]
        face=sex+'_Face_01'
        asset=appearance('P09_'+label,'P09 '+label,source,unreal.load_asset(mesh_paths[face]),[unreal.load_asset(mesh_paths[n]) for n in names if n!=face])
        factory=unreal.BlueprintFactory()
        factory.set_editor_property('parent_class',parent.generated_class())
        bp=own('BP_PGEnemy_P09_'+label,unreal.Blueprint,factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo=unreal.get_default_object(bp.generated_class())
        eid=15201+index
        cdo.set_editor_property('character_tid',eid)
        cdo.appearance_component.set_editor_property('default_appearance',asset)
        save(bp)
        row=copy.deepcopy(template)
        row.update(Name=str(eid),EnemyID=eid,EnemyName='P09 '+label,ActorClass=bp.generated_class().get_path_name())
        rows=[r for r in rows if r['EnemyID']!=eid]+[row]
        stat=copy.deepcopy(stat_template)
        stat.update(Name=str(eid),CharacterID=eid)
        stat_rows=[r for r in stat_rows if r['CharacterID']!=eid]+[stat]
        if death_template:
            death=copy.deepcopy(death_template)
            death.update(Name=str(eid),ObjectTID=eid)
            death_rows=[r for r in death_rows if r['ObjectTID']!=eid]+[death]
        REPORT['enemies'].append(dict(id=eid,asset=asset.get_path_name(),blueprint=bp.get_path_name(),parts=names,template_id=15101))
    # UE JSON exports numeric map values wrapped in the map property's name.
    for stat in stat_rows:
        stat['Stats']={k:(v.get('Stats',0) if isinstance(v,dict) else v) for k,v in stat['Stats'].items()}
    for table,contents in ((enemies,rows),(stat_table,stat_rows),(death_table,death_rows)):
        preserve(table.get_path_name())
        assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(contents,ensure_ascii=False))
        save(table)
    REPORT['status']='PASS'

try: main()
except Exception:
    REPORT.update(status='FAIL',error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    payload=json.dumps(REPORT,ensure_ascii=False,indent=2)
    (RUN/'configure.json').write_text(payload,encoding='utf-8')
    (OUT/'configure.json').write_text(payload,encoding='utf-8')
if REPORT['status']!='PASS': raise RuntimeError('Playable character configuration failed')
