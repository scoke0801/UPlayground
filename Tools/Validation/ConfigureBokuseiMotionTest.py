"""Import Anime Katana FBX and retarget directly to Bokusei in isolated folders.

Run with UE 5.8 PythonScriptPlugin commandlet. Source FBX and existing character,
gameplay, and gallery assets are read-only. Back up generated assets on reruns.
"""
import hashlib
import json
import math
import shutil
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
CONFIG = json.loads((ROOT/'Tools/Art/ToonTest/Bokusei/motion_manifest.json').read_text(encoding='utf-8'))
OUT = ROOT/'Saved/BokuseiMotion'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
LATEST = ROOT/'Saved/BokuseiMotion/configure.json'
REPORT = dict(status='RUNNING', run=str(OUT), clips=[])
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
DEST = CONFIG['destination']


def asset_file(path):
    return ROOT/'Content'/(path.split('.')[0].removeprefix('/Game/')+'.uasset')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def import_fbx(file, destination, name, skeleton=None):
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = str(file), destination, name
    task.automated, task.replace_existing, task.save = True, True, True
    task.factory = unreal.FbxFactory()
    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_materials = ui.import_textures = False
    ui.import_as_skeletal = True
    ui.import_mesh = skeleton is None
    ui.import_animations = skeleton is not None
    ui.create_physics_asset = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION if skeleton else unreal.FBXImportType.FBXIT_SKELETAL_MESH
    if skeleton:
        ui.skeleton = skeleton
        data = ui.anim_sequence_import_data
        data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        data.set_editor_property('use_default_sample_rate', True)
        data.convert_scene = True
        data.convert_scene_unit = True
    else:
        ui.skeletal_mesh_import_data.convert_scene = True
        ui.skeletal_mesh_import_data.convert_scene_unit = True
    task.options = ui
    TOOLS.import_asset_tasks([task])
    assets = [unreal.load_asset(p) for p in task.imported_object_paths]
    cls = unreal.AnimSequence if skeleton else unreal.SkeletalMesh
    asset = next((a for a in assets if isinstance(a, cls)), None)
    assert asset, (str(file), task.imported_object_paths)
    if isinstance(asset, unreal.SkeletalMesh):
        # AssetImportTask does not reliably persist the automatically generated
        # Skeleton; later animation-only imports require that separate package.
        generated_skeleton = asset.get_editor_property('skeleton')
        assert isinstance(generated_skeleton, unreal.Skeleton)
        save(generated_skeleton)
    save(asset)
    return asset


def own_asset(name, cls, factory):
    path = DEST+'/'+name
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else TOOLS.create_asset(name, DEST, cls, factory)
    assert isinstance(asset, cls), path
    return asset


def rig(name, mesh, pelvis, chains):
    asset = own_asset(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctl = unreal.IKRigController.get_controller(asset)
    assert ctl.set_skeletal_mesh(mesh)
    for chain in list(ctl.get_retarget_chains()):
        ctl.remove_retarget_chain(chain.chain_name)
    assert ctl.set_retarget_root(pelvis)
    for name, start, end in chains:
        assert str(ctl.add_retarget_chain(name, start, end, 'None')) == name
    save(asset)
    return asset


def skeleton_audit(mesh):
    comp = unreal.SkeletalMeshComponent()
    comp.set_skeletal_mesh_asset(mesh)
    temp = unreal.IKRigDefinition()
    ctl = unreal.IKRigController.get_controller(temp)
    assert ctl.set_skeletal_mesh(mesh)
    bones = []
    for i in range(comp.get_num_bones()):
        name = comp.get_bone_name(i)
        p = ctl.get_ref_pose_transform_of_bone(name).translation
        bones.append(dict(name=str(name), parent=str(comp.get_parent_bone(name)), position=[p.x,p.y,p.z]))
    return dict(mesh=mesh.get_path_name(), skeleton=mesh.get_editor_property('skeleton').get_path_name(), bones=bones)


def main():
    for folder in [DEST, CONFIG['source_destination']]:
        assert folder.startswith(('/Game/Art/ToonTest/Bokusei/Animation', '/Game/Art/AnimationTests/AnimeKatana'))
        local = ROOT/'Content'/folder.removeprefix('/Game/')
        if local.exists():
            shutil.copytree(local, OUT/'backup'/folder.removeprefix('/Game/'))
    source_root = Path(CONFIG['source_root'])
    files = [source_root/CONFIG['source_mesh_file']] + [source_root/'Animations/In_Place'/(c['file']+'.fbx') for c in CONFIG['clips']]
    protected = files + list((ROOT/'Content/Art/ToonTest/Bokusei').glob('*.uasset'))
    before = {str(p):digest(p) for p in protected}
    source = import_fbx(files[0], CONFIG['source_destination'], 'SK_PGAnimeKatana')
    target = unreal.load_asset(CONFIG['target_mesh'])
    assert isinstance(target, unreal.SkeletalMesh)
    for entry, file in zip(CONFIG['clips'], files[1:]):
        clip = import_fbx(file, CONFIG['source_destination']+'/Animations', entry['file'], source.get_editor_property('skeleton'))
        entry['source'] = clip.get_path_name()
    REPORT['skeletons'] = {'source':skeleton_audit(source), 'target':skeleton_audit(target)}
    (OUT/'skeletons.json').write_text(json.dumps(REPORT['skeletons'], indent=2), encoding='utf-8')
    src = [('Spine','spine_01','spine_05'), ('Neck','neck_01','neck_02'), ('Head','head','head')]
    dst = [('Spine','Spine','UpperChest'), ('Neck','Neck','Neck'), ('Head','Head','Head')]
    for side in ['l','r']:
        upper = side.upper()
        src += [('Shoulder_'+side,'clavicle_'+side,'clavicle_'+side), ('Arm_'+side,'upperarm_'+side,'hand_'+side),
                ('Leg_'+side,'thigh_'+side,'foot_'+side), ('Toe_'+side,'ball_'+side,'ball_'+side)]
        dst += [('Shoulder_'+side,'Shoulder_'+upper,'Shoulder_'+upper), ('Arm_'+side,'UpperArm_'+upper,'Hand_'+upper),
                ('Leg_'+side,'UpperLeg_'+upper,'Foot_'+upper), ('Toe_'+side,'Toes_'+upper,'Toes_'+upper)]
        for sf, tf in [('thumb','Thumb'),('index','Index'),('middle','Middle'),('ring','Ring'),('pinky','Little')]:
            src.append((sf+'_'+side,sf+'_01_'+side,sf+'_03_'+side))
            dst.append((sf+'_'+side,tf+'Proximal_'+upper,tf+'Distal_'+upper))
    sr = rig('IK_PGAnimeKatana_Source', source, 'pelvis', src)
    tr = rig('IK_PGBokusei_Target', target, 'Hips', dst)
    retarget = own_asset('RTG_PGAnimeKatana_Bokusei', unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctl = unreal.IKRetargeterController.get_controller(retarget)
    s, t = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
    ctl.remove_all_ops()
    ctl.set_ik_rig(s,sr); ctl.set_ik_rig(t,tr)
    ctl.set_preview_mesh(s,source); ctl.set_preview_mesh(t,target)
    for name in ['IKRetargetPelvisMotionOp','IKRetargetFKChainsOp']:
        index = ctl.add_retarget_op('/Script/IKRig.'+name)
        assert index >= 0
        ctl.run_op_initial_setup(index)
    ctl.assign_ik_rig_to_all_ops(s,sr); ctl.assign_ik_rig_to_all_ops(t,tr)
    for name,_,_ in dst:
        assert ctl.set_source_chain(name,name), name
    ctl.reset_retarget_pose('Default Pose',[],t)
    ctl.auto_align_all_bones(t, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    save(retarget)
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data(c['source']) for c in CONFIG['clips']]
    inputs.source_mesh, inputs.target_mesh, inputs.ik_retarget_asset = source,target,retarget
    inputs.target_path, inputs.prefix = DEST,'PGBokusei_'
    inputs.include_referenced_assets, inputs.overwrite_existing_files = False,True
    outputs = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    assert len(outputs) == len(CONFIG['clips'])
    watched = ['Hips','Head','Hand_L','Hand_R','Foot_L','Foot_R']
    options = unreal.AnimPoseEvaluationOptions()
    for entry in CONFIG['clips']:
        path = DEST+'/PGBokusei_'+entry['source'].split('.')[-1]
        clip = unreal.load_asset(path)
        assert isinstance(clip,unreal.AnimSequence) and clip.get_editor_property('skeleton') == target.get_editor_property('skeleton')
        save(clip)
        duration = clip.get_play_length()
        assert duration > 0
        frames = []
        for i in range(31):
            pose = clip.get_anim_pose_at_time(duration*i/30,options)
            frame = {}
            for bone in watched:
                p = unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD).translation
                frame[bone] = [p.x,p.y,p.z]
                assert all(math.isfinite(v) and abs(v)<10000 for v in frame[bone])
            frames.append(frame)
        travel = {b:max(math.dist(f[b],frames[0][b]) for f in frames) for b in watched}
        assert max(travel.values()) > .01
        REPORT['clips'].append(dict(entry,asset=path,duration=duration,max_bone_travel_cm=travel,sampled_positions=frames))
    assert all(digest(p)==h for p,h in before.items()), 'Protected source changed'
    REPORT.update(status='PASS', source_hashes=before, sources_unchanged=True, retargeter=retarget.get_path_name(),
                  map=CONFIG['map'], chain_count=len(dst), limitations=['FK + pelvis baseline; no IK foot locking',
                  'Secondary hair/cloth physics and gameplay integration are not part of this fixture'])


if __name__ == '__main__':
    try:
        main()
    except Exception:
        REPORT.update(status='FAIL',error=traceback.format_exc())
        unreal.log_error(REPORT['error'])
    finally:
        payload = json.dumps(REPORT,ensure_ascii=False,indent=2)
        (OUT/'configure.json').write_text(payload,encoding='utf-8')
        LATEST.write_text(payload,encoding='utf-8')
    if REPORT['status'] != 'PASS':
        raise RuntimeError('Bokusei retarget failed: '+str(LATEST))
    unreal.log('PG Bokusei direct retarget PASS')

