"""UE 5.8 offline retargeting; writes only the isolated toon test namespace.

Original player mesh, rigs, sequences and gameplay configuration are not edited.
Existing prototype outputs are backed up before regeneration.
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
CONFIG = json.loads((ROOT / 'Tools/Art/ToonTest/Inori/motion_manifest.json').read_text(encoding='utf-8'))
DEST = CONFIG['destination']
assert DEST.startswith('/Game/Art/ToonTest/')
RUN = ROOT / 'Saved/ToonTest/Retarget' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
RUN.mkdir(parents=True)
LATEST = ROOT / 'Saved/ToonTest/retarget.json'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
REPORT = {'status': 'RUNNING', 'run': str(RUN), 'clips': []}
LATEST.write_text(json.dumps(REPORT), encoding='utf-8')


def package_file(path):
    return ROOT / 'Content' / (path.split('.')[0].removeprefix('/Game/') + '.uasset')


def sha(path):
    return hashlib.sha256(package_file(path).read_bytes()).hexdigest()


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def own_asset(name, cls, factory):
    path = DEST + '/' + name
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not asset:
        asset = TOOLS.create_asset(name, DEST, cls, factory)
    assert isinstance(asset, cls), path
    return asset


def rig(name, mesh, pelvis, chains):
    asset = own_asset(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctl = unreal.IKRigController.get_controller(asset)
    assert ctl.set_skeletal_mesh(mesh)
    for chain in list(ctl.get_retarget_chains()):
        ctl.remove_retarget_chain(chain.chain_name)
    assert ctl.set_retarget_root(pelvis)
    for chain_name, start, end in chains:
        assert str(ctl.add_retarget_chain(chain_name, start, end, 'None')) == chain_name
    save(asset)
    return asset


def main():
    # Preserve previous prototype assets, not external sources.
    folder = ROOT / 'Content/Art/ToonTest/Inori/Animation'
    if folder.is_dir():
        shutil.copytree(folder, RUN / 'backup/Animation')
    sources = [CONFIG['source_mesh'], CONFIG['target_mesh']] + [c['source'] for c in CONFIG['clips']]
    before = {p: sha(p) for p in sources}
    src = unreal.load_asset(CONFIG['source_mesh'])
    dst = unreal.load_asset(CONFIG['target_mesh'])
    assert isinstance(src, unreal.SkeletalMesh) and isinstance(dst, unreal.SkeletalMesh)
    source_chains = [('Spine', 'spine_01', 'spine_05'), ('Neck', 'neck_01', 'neck_02'), ('Head', 'head', 'head')]
    target_chains = [('Spine', 'spine_01_x', 'spine_05_x'), ('Neck', 'c_subneck_1_x', 'neck_x'), ('Head', 'head_x', 'head_x')]
    for side in ['l', 'r']:
        source_chains += [('Clavicle_'+side, 'clavicle_'+side, 'clavicle_'+side),
                          ('Arm_'+side, 'upperarm_'+side, 'hand_'+side),
                          ('Leg_'+side, 'thigh_'+side, 'foot_'+side),
                          ('Toe_'+side, 'ball_'+side, 'ball_'+side)]
        target_chains += [('Clavicle_'+side, 'shoulder_'+side, 'shoulder_'+side),
                          ('Arm_'+side, 'arm_stretch_'+side, 'hand_'+side),
                          ('Leg_'+side, 'thigh_stretch_'+side, 'foot_'+side),
                          ('Toe_'+side, 'toes_01_'+side, 'toes_01_'+side)]
        for finger in ['thumb', 'index', 'middle', 'ring', 'pinky']:
            source_chains.append((finger+'_'+side, finger+'_01_'+side, finger+'_03_'+side))
            target_chains.append((finger+'_'+side, 'c_'+finger+'1_'+side, 'c_'+finger+'3_'+side))
    sr = rig('IK_PGToon_ElfSelena_Source', src, 'pelvis', source_chains)
    tr = rig('IK_PGToon_Inori_Target', dst, 'root_x', target_chains)
    retarget = own_asset('RTG_PGToon_ElfSelena_Inori', unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctl = unreal.IKRetargeterController.get_controller(retarget)
    source, target = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
    ctl.remove_all_ops()
    ctl.set_ik_rig(source, sr)
    ctl.set_ik_rig(target, tr)
    ctl.set_preview_mesh(source, src)
    ctl.set_preview_mesh(target, dst)
    # This first deformation test uses pelvis + FK. Foot planting and IK are not claimed.
    for struct in ['IKRetargetPelvisMotionOp', 'IKRetargetFKChainsOp']:
        index = ctl.add_retarget_op('/Script/IKRig.' + struct)
        assert index >= 0, struct
        ctl.run_op_initial_setup(index)
    ctl.assign_ik_rig_to_all_ops(source, sr)
    ctl.assign_ik_rig_to_all_ops(target, tr)
    for name, _, _ in target_chains:
        assert ctl.set_source_chain(name, name), name
    # Reset previous automatic pose before reruns.
    ctl.reset_retarget_pose('Default Pose', [], target)
    ctl.auto_align_all_bones(target, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    save(retarget)
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data(c['source']) for c in CONFIG['clips']]
    inputs.source_mesh, inputs.target_mesh, inputs.ik_retarget_asset = src, dst, retarget
    inputs.target_path, inputs.prefix = DEST, 'PGToon_'
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True
    outputs = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    assert len(outputs) == len(CONFIG['clips']), f'Expected 6 sequences, got {len(outputs)}'
    options = unreal.AnimPoseEvaluationOptions()
    pose_lib = unreal.AnimPoseExtensions
    watched = ['root_x', 'head_x', 'hand_l', 'hand_r', 'foot_l', 'foot_r']
    for entry in CONFIG['clips']:
        path = DEST + '/PGToon_' + entry['source'].split('/')[-1]
        clip = unreal.load_asset(path)
        assert isinstance(clip, unreal.AnimSequence), path
        assert clip.get_editor_property('skeleton') == dst.get_editor_property('skeleton'), path
        save(clip)
        duration = clip.get_play_length()
        assert duration > 0
        frames = []
        for fraction in [0, .25, .5, .75, 1]:
            pose = clip.get_anim_pose_at_time(duration * fraction, options)
            positions = {}
            for bone in watched:
                p = pose_lib.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
                assert all(math.isfinite(v) and abs(v) < 10000 for v in [p.x, p.y, p.z]), (path, bone)
                positions[bone] = [p.x, p.y, p.z]
            frames.append(positions)
        travel = {b: max(math.dist(f[b], frames[0][b]) for f in frames) for b in watched}
        assert max(travel.values()) > .01, 'Static output: ' + path
        REPORT['clips'].append({'label': entry['label'], 'asset': path, 'duration': duration,
                                'sample_fraction': entry['sample_fraction'], 'max_bone_travel_cm': travel,
                                'sampled_positions': frames})
    after = {p: sha(p) for p in sources}
    assert before == after, 'Source assets unexpectedly changed'
    REPORT.update(status='PASS', retargeter=retarget.get_path_name(), source_hashes=before,
                  sources_unchanged=True, chain_count=len(target_chains),
                  limitations=['FK-only; no IK foot planting', 'No hair/skirt secondary simulation',
                               'No gameplay AnimBP or player replacement'])


try:
    main()
except Exception:
    REPORT.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    payload = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (RUN / 'retarget.json').write_text(payload, encoding='utf-8')
    LATEST.write_text(payload, encoding='utf-8')
if REPORT['status'] != 'PASS':
    raise RuntimeError('PG Toon retarget failed; see ' + str(LATEST))
unreal.log('PG Toon retarget PASS: ' + str(LATEST))
