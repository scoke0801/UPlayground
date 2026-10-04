"""Expand the approved direct Anime Katana -> Bokusei transfer to all 60 clips.

Keeps the six approved sequences, original rig and original preview map byte intact.
Root-motion variants generate planar motion from the animated pelvis on Armature.
"""
import hashlib
import json
import math
import shutil
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
import ConfigureBokuseiMotionTest as shared

CONFIG = json.loads((ROOT/'Tools/Art/ToonTest/Bokusei/motion_library_manifest.json').read_text(encoding='utf-8'))
OUT = ROOT/'Saved/BokuseiMotionLibrary'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
REPORT = dict(status='RUNNING',run=str(OUT),clips=[],pages=[])
LATEST = ROOT/'Saved/BokuseiMotionLibrary/configure.json'
DEST = CONFIG['destination']
BASE_RETARGET = DEST+'/RTG_PGAnimeKatana_Bokusei'
RM_RETARGET = CONFIG['setup_destination']+'/RTG_PGAnimeKatana_Bokusei_RootMotion'
POSE = unreal.AnimPoseExtensions
OPTIONS = unreal.AnimPoseEvaluationOptions()
WATCHED = ['Armature','Hips','Head','Hand_L','Hand_R','Foot_L','Foot_R']
LIMBS = [('UpperArm_L','LowerArm_L'),('LowerArm_L','Hand_L'),('UpperArm_R','LowerArm_R'),('LowerArm_R','Hand_R'),
         ('UpperLeg_L','LowerLeg_L'),('LowerLeg_L','Foot_L'),('UpperLeg_R','LowerLeg_R'),('LowerLeg_R','Foot_R')]


def vec(p):
    return [p.x,p.y,p.z]


def positions(clip, fraction, bones):
    pose = clip.get_anim_pose_at_time(clip.get_play_length()*fraction, OPTIONS)
    return {b:vec(POSE.get_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD).translation) for b in bones}


def backup(path):
    file = shared.asset_file(path)
    if file.exists():
        destination = OUT/'backup'/file.relative_to(ROOT/'Content')
        destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(file,destination)


def main():
    base_manifest = json.loads((ROOT/'Tools/Art/ToonTest/Bokusei/motion_manifest.json').read_text())
    files = [Path(CONFIG['source_root'])/'Animations'/c['group']/(c['file']+'.fbx') for c in CONFIG['clips']]
    protected = files + [Path(CONFIG['source_root'])/CONFIG['source_mesh_file']] + list((ROOT/'Content/Art/ToonTest/Bokusei').glob('*.uasset'))
    protected += [shared.asset_file(DEST+'/PGBokusei_'+c['file']) for c in base_manifest['clips']]
    protected += [shared.asset_file(CONFIG['source_destination']+'/Animations/'+c['file']) for c in base_manifest['clips']]
    protected += [shared.asset_file(DEST+'/'+n) for n in ['IK_PGAnimeKatana_Source','IK_PGBokusei_Target','RTG_PGAnimeKatana_Bokusei']]
    protected += list((ROOT/'Content/Art/AnimationTests/AnimeKatana').glob('*.uasset'))
    protected += [ROOT/'Content/Art/ToonTest/Maps/L_PGToon_Bokusei_MotionTest.umap']
    before = {str(p):shared.digest(p) for p in protected}
    # The initial sample import persisted the mesh but omitted its generated
    # source Skeleton package. Reconstruct only that missing dependency, keeping
    # every approved mesh/clip/rig byte unchanged.
    source_skeleton_path = CONFIG['source_mesh']+'_Skeleton'
    if not unreal.EditorAssetLibrary.does_asset_exist(source_skeleton_path):
        recovery_dest = CONFIG['source_destination']+'/LibrarySetup'
        recovery = shared.import_fbx(Path(CONFIG['source_root'])/CONFIG['source_mesh_file'],
                                    recovery_dest,'SK_PGAnimeKatana_SourceRecovery')
        skeleton = recovery.get_editor_property('skeleton')
        assert isinstance(skeleton,unreal.Skeleton)
        shared.save(skeleton)
        assert unreal.EditorAssetLibrary.rename_asset(skeleton.get_path_name(),source_skeleton_path)
        shared.save(skeleton)
        shared.save(recovery)
        REPORT['repaired_missing_source_skeleton'] = source_skeleton_path
    source = unreal.load_asset(CONFIG['source_mesh'])
    target = unreal.load_asset(CONFIG['target_mesh'])
    assert isinstance(source,unreal.SkeletalMesh) and isinstance(target,unreal.SkeletalMesh)
    assert isinstance(source.get_editor_property('skeleton'),unreal.Skeleton),'Source skeleton missing'
    for entry,file in zip(CONFIG['clips'],files):
        raw_dest = CONFIG['source_destination']+'/Animations'+('/RootMotion' if entry['group']=='Root_Motion' else '')
        raw_path = raw_dest+'/'+entry['file']
        clip = unreal.load_asset(raw_path) if unreal.EditorAssetLibrary.does_asset_exist(raw_path) else shared.import_fbx(file,raw_dest,entry['file'],source.get_editor_property('skeleton'))
        assert isinstance(clip,unreal.AnimSequence)
        entry['source'] = clip.get_path_name()
        entry['asset'] = (CONFIG['root_destination'] if entry['group']=='Root_Motion' else DEST)+'/PGBokusei_'+entry['file']
        if not entry['preserve_existing']:
            backup(entry['asset'])
    backup(RM_RETARGET)
    if unreal.EditorAssetLibrary.does_asset_exist(RM_RETARGET):
        rm = unreal.load_asset(RM_RETARGET)
    else:
        rm = unreal.EditorAssetLibrary.duplicate_asset(BASE_RETARGET,RM_RETARGET)
    assert isinstance(rm,unreal.IKRetargeter)
    ctl = unreal.IKRetargeterController.get_controller(rm)
    # This derived setup owns only its appended root op; preserve approved FK setup.
    if ctl.get_num_retarget_ops() == 2:
        index = ctl.add_retarget_op('/Script/IKRig.IKRetargetRootMotionOp')
        assert index == 2
        ctl.run_op_initial_setup(index)
    root_ctl = ctl.get_op_controller(2)
    assert isinstance(root_ctl,unreal.IKRetargetRootMotionController)
    root_ctl.set_source_root_bone('root')
    root_ctl.set_target_root_bone('Armature')
    root_ctl.set_target_pelvis_bone('Hips')
    settings = root_ctl.get_settings()
    settings.set_editor_property('root_motion_source',unreal.RootMotionSource.GENERATE_FROM_TARGET_PELVIS)
    settings.set_editor_property('root_height_source',unreal.RootMotionHeightSource.SNAP_TO_GROUND)
    settings.set_editor_property('maintain_offset_from_pelvis',False)
    settings.set_editor_property('rotate_with_pelvis',False)
    root_ctl.set_settings(settings)
    shared.save(rm)
    for group in ['In_Place','Root_Motion']:
        entries = [c for c in CONFIG['clips'] if c['group']==group and not c['preserve_existing']]
        inputs = unreal.IKRetargetBatchOperationInputs()
        inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data(c['source']) for c in entries]
        inputs.source_mesh,inputs.target_mesh = source,target
        inputs.ik_retarget_asset = rm if group=='Root_Motion' else unreal.load_asset(BASE_RETARGET)
        inputs.target_path = CONFIG['root_destination'] if group=='Root_Motion' else DEST
        inputs.prefix = 'PGBokusei_'
        inputs.include_referenced_assets,inputs.overwrite_existing_files = False,True
        assert len(unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)) == len(entries)
    for entry in CONFIG['clips']:
        clip = unreal.load_asset(entry['asset'])
        original = unreal.load_asset(entry['source'])
        assert isinstance(clip,unreal.AnimSequence) and clip.get_editor_property('skeleton') == target.get_editor_property('skeleton')
        duration = clip.get_play_length()
        assert duration > 0 and abs(duration-original.get_play_length()) < .001
        # Validate the unextracted trajectory before enabling root-motion extraction.
        if not entry['preserve_existing']:
            clip.set_editor_property('enable_root_motion',False)
            clip.set_editor_property('force_root_lock',False)
        frames = [positions(clip,i/30,WATCHED) for i in range(31)]
        assert all(math.isfinite(v) and abs(v)<10000 for f in frames for p in f.values() for v in p)
        travel = {b:max(math.dist(f[b],frames[0][b]) for f in frames) for b in WATCHED}
        probe = max(range(31), key=lambda i:max(math.dist(frames[i][b],frames[0][b]) for b in WATCHED[2:]))
        lengths = [[math.dist(p[a],p[b]) for a,b in LIMBS] for p in [positions(clip,i/20,sorted({b for pair in LIMBS for b in pair})) for i in range(21)]]
        variation = max(max(f[i] for f in lengths)-min(f[i] for f in lengths) for i in range(len(LIMBS)))
        assert variation<.01,(entry['file'],variation)
        row = dict(entry,duration=duration,max_bone_travel_cm=travel,max_limb_length_variation_cm=variation,
                   probe_fraction=probe/30,static_pose=max(travel.values())<.01)
        if entry['group']=='Root_Motion':
            source_frames = [positions(original,i/30,['pelvis'])['pelvis'] for i in range(31)]
            source_frames = [[p[0],p[1],0] for p in source_frames]
            source_travel = max(math.dist(p,source_frames[0]) for p in source_frames)
            assert source_travel>1 and travel['Armature']>1, ('Missing root translation',entry['file'],source_travel,travel['Armature'])
            ratio = travel['Armature']/source_travel
            assert .5<ratio<1.5,(entry['file'],ratio)
            trajectory_error=max(math.dist([f['Armature'][j]-frames[0]['Armature'][j] for j in range(3)],
                                           [(p[j]-source_frames[0][j])*ratio for j in range(3)])
                                 for f,p in zip(frames,source_frames))
            assert trajectory_error<.1,(entry['file'],'Root trajectory differs',trajectory_error)
            row.update(source_root_travel_cm=source_travel,root_translation_scale=ratio,
                       root_positions=[f['Armature'] for f in frames],source_root_positions=source_frames,
                       root_trajectory_error_cm=trajectory_error,root_source='pelvis XY; vertical/rotation retained on Hips')
            clip.set_editor_property('enable_root_motion',True)
            clip.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
        if not entry['preserve_existing']:
            shared.save(clip)
        REPORT['clips'].append(row)
    for group,short in [('In_Place','IP'),('Root_Motion','RM')]:
        entries = [c for c in REPORT['clips'] if c['group']==group]
        for i in range(0,len(entries),6):
            REPORT['pages'].append(dict(name=f'Katana_{short}_{i//6+1:02d}',group=group,
                map=CONFIG['maps_destination']+f'/L_PGBokusei_Katana_{short}_{i//6+1:02d}',clips=entries[i:i+6]))
    assert all(shared.digest(p)==h for p,h in before.items()), 'Approved/source asset changed'
    REPORT.update(status='PASS',source_hashes=before,sources_unchanged=True,root_retargeter=RM_RETARGET,
                  total=60,added=54,in_place=40,root_motion=20,
                  limitations=['FK baseline; no contact IK, weapon grip or secondary cloth physics',
                               'Root-motion extraction enabled; planar pelvis motion generated on Armature, vertical/rotation stay on Hips',
                               'Gameplay movement integration remains separate'])


if __name__=='__main__':
    try:
        main()
    except Exception:
        REPORT.update(status='FAIL',error=traceback.format_exc())
        unreal.log_error(REPORT['error'])
    finally:
        payload=json.dumps(REPORT,ensure_ascii=False,indent=2)
        (OUT/'configure.json').write_text(payload,encoding='utf-8')
        LATEST.write_text(payload,encoding='utf-8')
    if REPORT['status']!='PASS':
        raise RuntimeError('Bokusei library failed: '+str(LATEST))
    unreal.log('PG Bokusei library PASS: 60 clips')
