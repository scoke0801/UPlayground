"""Create isolated source meshes/rigs for the three approved Bokusei packs."""
import json
import sys
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
import ConfigureBokuseiMotionTest as shared
from AuditBokuseiPacks import SOURCE,PACKS

CATALOG={
 'Frank2Handed':('FrankSlash','Assets/Meshes/Frank_2Handed_Skin.FBX'),
 'FrankAssassin':('FrankSlash','Assets/Meshes/Frank_Assassin_Skin.FBX'),
 'FrankDual':('FrankSlash','Assets/Meshes/Frank_Dual_Skin.FBX'),
 'FrankKatana':('FrankSlash','Assets/Meshes/Frank_Katana_Skin.FBX'),
 'FrankGreatSword':('FrankSlash','Assets/Meshes/Frank_RPG_GreatSword.FBX'),
 'FrankSpear':('FrankSlash','Assets/Meshes/Frank_RPG_Spear_Unity.FBX'),
 'FrankSword2':('FrankSlash','Assets/Meshes/Frank_Sword2_Skin.FBX'),
 'FrankDamage':('FrankSlash','Assets/Meshes/Frank_Sword2_Damage_Man_Skin.FBX'),
 'FrankWarrior':('FrankSlash','Assets/Meshes/Frank_Warrior_Skin.FBX'),
 'FrankWhip':('FrankSlash','Assets/Meshes/SK_Frank_Whip_WithCam.FBX'),
 'Grruzam':('GrruzamSword','Modeling/Modeling_T-Pose_Grrrru_Man(recommend).FBX'),
 'RPG':('RPGAnimations','Model/T-Pose.fbx'),
}
OUT=ROOT/'Saved/BokuseiPacks'
OUT.mkdir(parents=True,exist_ok=True)
REPORT=dict(status='RUNNING',sources={})


def make_rigs(key,mesh,target,target_rig,destination,audit):
    shared.DEST=destination
    rpg=key=='RPG'
    pelvis,root=('Hips','Skeleton') if rpg else ('pelvis','root')
    chains=[('Spine','Spine','UpperChest'),('Neck','Neck','Neck'),('Head','Head','Head')] if rpg else [
        ('Spine','spine_01','spine_03'),('Neck','neck_01','neck_01'),('Head','head','head')]
    for side,long_side in [('l','Left'),('r','Right')]:
        if rpg:
            chains += [('Shoulder_'+side,long_side+'_Shoulder',long_side+'_Shoulder'),
                       ('Arm_'+side,long_side+'_UpperArm',long_side+'_Hand'),
                       ('Leg_'+side,long_side+'_UpperLeg',long_side+'_Foot'),
                       ('Toe_'+side,long_side+'_Toes',long_side+'_Toes')]
        else:
            chains += [('Shoulder_'+side,'clavicle_'+side,'clavicle_'+side),('Arm_'+side,'upperarm_'+side,'hand_'+side),
                       ('Leg_'+side,'thigh_'+side,'foot_'+side),('Toe_'+side,'ball_'+side,'ball_'+side)]
        for finger,human in [('thumb','Thumb'),('index','Index'),('middle','Middle'),('ring','Ring'),('pinky','Pinky')]:
            chains.append((finger+'_'+side,long_side+'_'+human+'Proximal',long_side+'_'+human+'Distal') if rpg else
                          (finger+'_'+side,finger+'_01_'+side,finger+'_03_'+side))
    names={b['name'] for b in audit['bones']}
    assert all(a in names and b in names for _,a,b in chains),(key,chains)
    source_rig=shared.rig('IK_PG'+key,mesh,pelvis,chains)
    retarget=shared.own_asset('RTG_PG'+key+'_Bokusei',unreal.IKRetargeter,unreal.IKRetargetFactory())
    ctl=unreal.IKRetargeterController.get_controller(retarget)
    s,t=unreal.RetargetSourceOrTarget.SOURCE,unreal.RetargetSourceOrTarget.TARGET
    ctl.remove_all_ops()
    ctl.set_ik_rig(s,source_rig);ctl.set_ik_rig(t,target_rig)
    ctl.set_preview_mesh(s,mesh);ctl.set_preview_mesh(t,target)
    for op in ['IKRetargetPelvisMotionOp','IKRetargetFKChainsOp']:
        index=ctl.add_retarget_op('/Script/IKRig.'+op)
        ctl.run_op_initial_setup(index)
    ctl.assign_ik_rig_to_all_ops(s,source_rig);ctl.assign_ik_rig_to_all_ops(t,target_rig)
    for name,_,_ in chains:
        assert ctl.set_source_chain(name,name)
    ctl.reset_retarget_pose('Default Pose',[],t)
    ctl.auto_align_all_bones(t,unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    shared.save(retarget)
    paths={'Body':retarget.get_path_name()}
    for mode in ['Root','PelvisXY']:
        path=destination+'/RTG_PG'+key+'_Bokusei_'+mode
        rm=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(retarget.get_path_name(),path)
        controller=unreal.IKRetargeterController.get_controller(rm)
        if controller.get_num_retarget_ops()==2:
            index=controller.add_retarget_op('/Script/IKRig.IKRetargetRootMotionOp')
            controller.run_op_initial_setup(index)
        op=controller.get_op_controller(2)
        op.set_source_root_bone(root);op.set_target_root_bone('Armature');op.set_target_pelvis_bone('Hips')
        settings=op.get_settings()
        if mode=='PelvisXY':
            settings.set_editor_property('root_motion_source',unreal.RootMotionSource.GENERATE_FROM_TARGET_PELVIS)
            settings.set_editor_property('root_height_source',unreal.RootMotionHeightSource.SNAP_TO_GROUND)
            settings.set_editor_property('maintain_offset_from_pelvis',False)
            settings.set_editor_property('rotate_with_pelvis',False)
        op.set_settings(settings)
        shared.save(rm)
        paths[mode]=path
    return dict(rig=source_rig.get_path_name(),retargeters=paths,pelvis=pelvis,root=root)


def main():
    target=unreal.load_asset('/Game/Art/ToonTest/Bokusei/SK_Bokusei_ToonTest')
    target_rig=unreal.load_asset('/Game/Art/ToonTest/Bokusei/Animation/IK_PGBokusei_Target')
    for key,(pack,relative) in CATALOG.items():
        destination='/Game/Art/AnimationTests/'+pack+'/Setup/'+key
        mesh_path=destination+'/SK_PG'+key
        mesh=unreal.load_asset(mesh_path) if unreal.EditorAssetLibrary.does_asset_exist(mesh_path) else shared.import_fbx(SOURCE/PACKS[pack]/relative,destination,'SK_PG'+key)
        audit=shared.skeleton_audit(mesh)
        setup=make_rigs(key,mesh,target,target_rig,destination,audit)
        REPORT['sources'][key]=dict(audit,pack=pack,file=str(SOURCE/PACKS[pack]/relative),mesh=mesh_path,**setup)
        (OUT/'setup.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    REPORT['status']='PASS'


try:
    main()
except Exception:
    REPORT.update(status='FAIL',error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT/'setup.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
if REPORT['status']!='PASS':
    raise RuntimeError(REPORT['error'])
