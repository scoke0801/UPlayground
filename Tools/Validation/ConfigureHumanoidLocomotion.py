"""Author isolated sword locomotion; preserve source/skeleton enemies and all combat data."""
import hashlib
import json
import math
import os
from pathlib import Path
import re
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from PlayableCharacterTransaction import Transaction, write_json

SPEC_FILE = ROOT/'Tools/Validation/Data/HumanoidLocomotion.json'
SPEC = json.loads(SPEC_FILE.read_text(encoding='utf-8'))
DEST = SPEC['destination']
RUN = Path(os.environ.get('PG_LOCOMOTION_RUN',str(ROOT/'Saved/HumanoidLocomotion')))
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
TX = None
DIRECTIONS = [(-180,'B'),(-135,'BL'),(-90,'L'),(-45,'FL'),(0,'F'),(45,'FR'),(90,'R'),(135,'BR'),(180,'B')]
ENEMY_BPS = ['/Game/DataCenter/MonsterVariations/BP_'+str(i) for i in range(15201,15207)]
ENEMY_BPS += ['/Game/DataCenter/Characters/BP_PGEnemy_P09_'+s for s in ('Female','Male','Female_Armor007','Male_Armor007')]

def own(name, cls, factory):
    path = DEST+'/'+name
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(name, DEST, cls, factory)

def save(asset):
    TX.mark_written(asset.get_path_name().split('.')[0])
    assert EAL.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()

def source_entries(pack, all_directions, player=False):
    folder = '/Game/Art/AnimationTests/FrankSlash/Animations/'+pack
    paths = [str(a.package_name) for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder)]
    entries = {}
    for key, prefix in SPEC['packs'][pack].items():
        if key=='mesh': continue
        if player and key=='idle': prefix=SPEC['player_idle_prefix']
        suffixes = [''] if key=='idle' else (sorted({d for _,d in DIRECTIONS}) if all_directions else ['F'])
        for direction in suffixes:
            pattern = prefix+(direction+'_' if direction else '')+'[0-9a-z]{10}'
            found = [p for p in paths if re.fullmatch(pattern, p.rsplit('/',1)[1])]
            assert len(found)==1, (pack,key,direction,found)
            entries[key+('_'+direction if direction else '')] = found[0]
    return entries

def rig(name, mesh):
    asset = own(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctl = unreal.IKRigController.get_controller(asset)
    assert ctl.set_skeletal_mesh(mesh)
    for chain in list(ctl.get_retarget_chains()): ctl.remove_retarget_chain(chain.chain_name)
    assert ctl.set_retarget_root('pelvis')
    comp = unreal.SkeletalMeshComponent(); comp.set_skeletal_mesh_asset(mesh)
    names = {str(comp.get_bone_name(i)).lower():str(comp.get_bone_name(i)) for i in range(comp.get_num_bones())}
    chains = [('Spine','spine_01','spine_05' if 'spine_05' in names else 'spine_03'),
              ('Neck','neck_01','neck_02' if 'neck_02' in names else 'neck_01'),('Head','head','head')]
    for side in ('l','r'):
        chains += [(f'Shoulder_{side}',f'clavicle_{side}',f'clavicle_{side}'),(f'Arm_{side}',f'upperarm_{side}',f'hand_{side}'),
                   (f'Leg_{side}',f'thigh_{side}',f'foot_{side}'),(f'Toe_{side}',f'ball_{side}',f'ball_{side}')]
        for finger in ('thumb','index','middle','ring','pinky'):
            if f'{finger}_01_{side}' in names: chains.append((f'{finger}_{side}',f'{finger}_01_{side}',f'{finger}_03_{side}'))
    for label,start,end in chains:
        assert str(ctl.add_retarget_chain(label,names[start],names[end],'None'))==label
    save(asset)
    return asset, chains

def retarget(label, pack, target, entries):
    meshname = SPEC['packs'][pack]['mesh']
    source = unreal.load_asset('/Game/Art/AnimationTests/FrankSlash/Setup/'+meshname+'/SK_PG'+meshname)
    sr, _ = rig('IK_'+label+'_Source', source)
    tr, chains = rig('IK_'+label+'_Target', target)
    rt = own('RTG_'+label, unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctl = unreal.IKRetargeterController.get_controller(rt)
    s,t = unreal.RetargetSourceOrTarget.SOURCE,unreal.RetargetSourceOrTarget.TARGET
    ctl.remove_all_ops();ctl.set_ik_rig(s,sr);ctl.set_ik_rig(t,tr)
    ctl.set_preview_mesh(s,source);ctl.set_preview_mesh(t,target)
    for name in ('IKRetargetPelvisMotionOp','IKRetargetFKChainsOp'):
        index=ctl.add_retarget_op('/Script/IKRig.'+name);assert index>=0;ctl.run_op_initial_setup(index)
    ctl.assign_ik_rig_to_all_ops(s,sr);ctl.assign_ik_rig_to_all_ops(t,tr)
    for name,_,_ in chains: assert ctl.set_source_chain(name,name)
    ctl.reset_retarget_pose('Default Pose',[],t)
    ctl.auto_align_all_bones(t,unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    save(rt)
    inputs=unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget=[EAL.find_asset_data(p) for p in entries.values()]
    inputs.source_mesh=source;inputs.target_mesh=target;inputs.ik_retarget_asset=rt
    inputs.target_path=DEST;inputs.prefix=label+'_'
    inputs.include_referenced_assets=False;inputs.overwrite_existing_files=True
    assert len(unreal.IKRetargetBatchOperation.run_batch_retarget(inputs))==len(entries)
    clips={}
    for key,path in entries.items():
        clip=unreal.load_asset(DEST+'/'+label+'_'+path.rsplit('/',1)[1]);assert clip
        clip.set_editor_property('enable_root_motion',False)
        clip.set_editor_property('force_root_lock',True)
        clip.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
        unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
        save(clip);clips[key]=clip
    return clips

def authored_rate(pack, key, target, speed):
    if key=='idle':return 1.
    movement,direction=key.split('_')
    prefix=(SPEC['packs'][pack][movement]+direction+'_RootMotion_' if pack=='Sword2'
            else 'AS_PGVelocity_8Way_'+movement.title()+'_'+direction+'_')
    paths=[str(a.package_name) for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/Art/AnimationTests/FrankSlash/Animations/'+pack)]
    matches=[p for p in paths if re.fullmatch(prefix+'[0-9a-z]{10}',p.rsplit('/',1)[1])]
    assert len(matches)==1,(pack,key,matches)
    source=unreal.load_asset(matches[0]);opts=unreal.AnimPoseEvaluationOptions()
    first=source.get_anim_pose_at_time(0.,opts);last=source.get_anim_pose_at_time(source.get_play_length(),opts)
    def point(pose,bone):return unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD).translation
    delta=point(last,'root')-point(first,'root')
    original=math.hypot(delta.x,delta.y)/source.get_play_length()
    target_pose=target.get_anim_pose_at_time(0.,opts)
    scale=point(target_pose,'pelvis').z/point(first,'pelvis').z
    rate=speed/(original*scale)
    assert .4<rate<2.5,(pack,key,original,scale,rate)
    return rate

def blend(name, clips, directional, walk_speed, run_speed, pack):
    factory=unreal.BlendSpaceFactoryNew() if directional else unreal.BlendSpaceFactory1D()
    factory.set_editor_property('target_skeleton',clips['idle'].get_editor_property('skeleton'))
    asset=own(name,unreal.BlendSpace if directional else unreal.BlendSpace1D,factory)
    speed=unreal.BlendParameter();speed.set_editor_property('display_name','Speed');speed.set_editor_property('min',0.);speed.set_editor_property('max',float(run_speed))
    direction=unreal.BlendParameter();direction.set_editor_property('display_name','Direction');direction.set_editor_property('min',-180.);direction.set_editor_property('max',180.);direction.set_editor_property('grid_num',8);direction.set_editor_property('wrap_input',True)
    asset.set_editor_property('blend_parameters',[direction,speed,speed] if directional else [speed,speed,speed])
    samples=[]
    for angle,suffix in DIRECTIONS if directional else [(0,'F')]:
        for velocity,key in [(0,'idle'),(walk_speed,'walk_'+suffix),(run_speed,'run_'+suffix)]:
            sample=unreal.BlendSample();sample.set_editor_property('animation',clips[key])
            sample.set_editor_property('rate_scale',authored_rate(pack,key,clips[key],velocity))
            sample.set_editor_property('sample_value',unreal.Vector(angle,velocity,0) if directional else unreal.Vector(velocity,0,0))
            samples.append(sample)
    asset.set_editor_property('sample_data',samples)
    asset.set_editor_property('target_weight_interpolation_speed_per_sec',8.)
    unreal.PGHumanoidLocomotionTools.rebuild_blend_space(asset)
    save(asset)
    return asset

def configure_enemy(cdo, shield=False):
    asset=unreal.load_asset(DEST+('/BS_P09_Shield8Way' if shield else '/BS_P09_Sword8Way'))
    assert asset, 'Generate humanoid locomotion before regenerating P09 actors'
    assert asset.get_editor_property('skeleton')==cdo.mesh.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
    cdo.set_editor_property('creature_locomotion',asset)
    cdo.mesh.set_editor_property('anim_class',unreal.PGCreatureAnimInstance.static_class())

def protected_hashes():
    folders=['Blueprints/Actor/NonPlayer/Enemy/Skeleton','ExternalAssets/Characters/Enemies/SkeletonEnemy',
             'ExternalAssets/Animations/Unarmed','DataCenter/DataTables','DataCenter/Characters',
             'DataCenter/HumanoidBoss','DataCenter/HackSlashP0','DataCenter/HackSlashP1']
    excluded={p.removeprefix('/Game/')+'.uasset' for p in ENEMY_BPS}
    files=[p for folder in folders for p in (ROOT/'Content'/folder).rglob('*.uasset') if p.relative_to(ROOT/'Content').as_posix() not in excluded]
    return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}

def validate():
    report={'status':'PASS','blends':[],'enemies':[],'clips':[]}
    for name,count in [('BS_PlayerSword',27),('BS_P09_Sword8Way',27),('BS_P09_Shield8Way',27)]:
        asset=unreal.load_asset(DEST+'/'+name);assert asset
        samples=list(asset.get_editor_property('sample_data'));assert len(samples)==count
        for sample in samples:
            clip=sample.get_editor_property('animation');assert clip and clip.get_path_name().startswith(DEST+'/')
            assert clip.get_editor_property('skeleton')==asset.get_editor_property('skeleton')
            assert unreal.PGHumanoidLocomotionTools.get_blend_sample_count(asset,sample.get_editor_property('sample_value'))>0,'Unbuilt blend sample: '+name
        report['blends'].append(dict(name=name,samples=count))
    for path in ENEMY_BPS:
        cdo=unreal.get_default_object(unreal.load_asset(path).generated_class())
        assert cdo.mesh.get_editor_property('anim_class')==unreal.PGCreatureAnimInstance.static_class()
        expected=DEST+('/BS_P09_Shield8Way' if path.endswith(('15205','15206')) else '/BS_P09_Sword8Way')
        assert cdo.get_editor_property('creature_locomotion')==unreal.load_asset(expected)
        report['enemies'].append(path)
    bp=unreal.load_asset(SPEC['player_blueprint'])
    node=unreal.find_object(None,bp.get_path_name()+':AnimGraph.PGHumanoidGround')
    assert node and node.get_editor_property('node').get_editor_property('blend_space')==unreal.load_asset(DEST+'/BS_PlayerSword')
    for entry in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(DEST):
        if str(entry.asset_class_path.asset_name)!='AnimSequence':continue
        clip=unreal.load_asset(str(entry.package_name));assert clip.get_play_length()>0
        assert not clip.get_editor_property('enable_root_motion') and clip.get_editor_property('force_root_lock')
        assert not unreal.AnimationLibrary.get_animation_notify_events(clip)
        frames=[]
        for fraction in (0,.25,.5,.75,1):
            pose=clip.get_anim_pose_at_time(clip.get_play_length()*fraction,unreal.AnimPoseEvaluationOptions())
            points=[]
            for bone in ('pelvis','foot_l','foot_r','hand_l','hand_r','head'):
                p=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD).translation
                assert all(math.isfinite(v) and abs(v)<1000 for v in (p.x,p.y,p.z))
                points.append([p.x,p.y,p.z])
            frames.append(points)
        assert max(math.dist(a,b) for frame in frames for a,b in zip(frame,frames[0]))>.1
        report['clips'].append(dict(path=clip.get_path_name(),seconds=clip.get_play_length(),samples=frames))
    assert len(report['clips'])>=51  # Previous generated idle may remain for rollback/reference.
    player_samples=unreal.load_asset(DEST+'/BS_PlayerSword').get_editor_property('sample_data')
    assert all(SPEC['player_idle_prefix'] in s.get_editor_property('animation').get_name()
               for s in player_samples if s.get_editor_property('sample_value').y==0)
    before=json.loads((RUN/'protected.json').read_text(encoding='utf-8'))
    assert before==protected_hashes(),'Protected skeleton/combat/appearance asset changed'
    report['protected_packages']=len(before)
    write_json(RUN/'validation.json',report)
    unreal.log('PGHumanoidLocomotion VALIDATION PASS')

def apply():
    global TX
    groups=[('Player','Sword2',SPEC['player_mesh'],True),('P09','Sword2',SPEC['enemy_mesh'],True),('Shield','Warrior',SPEC['enemy_mesh'],True)]
    entries={label:source_entries(pack,directional,label=='Player') for label,pack,_,directional in groups}
    packages=ENEMY_BPS+[SPEC['player_blueprint']]+[DEST+'/'+name for name in ('BS_PlayerSword','BS_P09_Sword8Way','BS_P09_Shield8Way')]
    for label,_,_,_ in groups:
        packages += [DEST+'/'+p+label+s for p,s in [('IK_','_Source'),('IK_','_Target'),('RTG_','')]]
        packages += [DEST+'/'+label+'_'+p.rsplit('/',1)[1] for p in entries[label].values()]
    TX=Transaction(ROOT,RUN);TX.prepare(packages,SPEC_FILE)
    write_json(RUN/'protected.json',protected_hashes())
    for label,pack,mesh,directional in groups:
        clips=retarget(label,pack,unreal.load_asset(mesh),entries[label])
        name={'Player':'BS_PlayerSword','P09':'BS_P09_Sword8Way','Shield':'BS_P09_Shield8Way'}[label]
        blend(name,clips,directional,SPEC['player_walk_speed' if label=='Player' else 'enemy_walk_speed'],
              SPEC['player_run_speed' if label=='Player' else 'shield_run_speed' if label=='Shield' else 'enemy_run_speed'],pack)
    bp=unreal.load_asset(SPEC['player_blueprint'])
    assert unreal.PGHumanoidLocomotionTools.configure_player_ground_locomotion(bp,unreal.load_asset(DEST+'/BS_PlayerSword'))
    save(bp)
    for path in ENEMY_BPS:
        bp=unreal.load_asset(path)
        configure_enemy(unreal.get_default_object(bp.generated_class()),path.endswith(('15205','15206')))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    TX.data['status']='APPLIED_UNVERIFIED';TX.flush()
    validate()
    unreal.log('PGHumanoidLocomotion APPLY PASS')

if __name__=='__main__':
    validate() if '-PGHumanoidLocomotionValidate' in unreal.SystemLibrary.get_command_line() else apply()
