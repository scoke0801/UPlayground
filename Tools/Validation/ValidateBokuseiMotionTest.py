"""Read-only validation of persisted gallery references and sampled deformation."""
import hashlib
import json
import math
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
REPORT = {'status': 'RUNNING', 'clips': [], 'actors': []}
path = ROOT / 'Saved/BokuseiMotion/validation.json'
audit = json.loads((ROOT / 'Saved/BokuseiMotion/configure.json').read_text(encoding='utf-8'))


def main():
    assert audit['status'] == 'PASS'
    for asset, expected in audit['source_hashes'].items():
        file = Path(asset)
        assert hashlib.sha256(file.read_bytes()).hexdigest() == expected, asset
    target = unreal.load_asset('/Game/Art/ToonTest/Bokusei/SK_Bokusei_ToonTest')
    options = unreal.AnimPoseEvaluationOptions()
    pairs = [('UpperArm_L', 'LowerArm_L'), ('LowerArm_L', 'Hand_L'),
             ('UpperArm_R', 'LowerArm_R'), ('LowerArm_R', 'Hand_R'),
             ('UpperLeg_L', 'LowerLeg_L'), ('LowerLeg_L', 'Foot_L'),
             ('UpperLeg_R', 'LowerLeg_R'), ('LowerLeg_R', 'Foot_R')]
    for entry in audit['clips']:
        clip = unreal.load_asset(entry['asset'])
        assert clip.get_editor_property('skeleton') == target.get_editor_property('skeleton')
        lengths = []
        for i in range(21):
            pose = clip.get_anim_pose_at_time(clip.get_play_length()*i/20, options)
            frame = []
            for a, b in pairs:
                transforms = [unreal.AnimPoseExtensions.get_bone_pose(pose, n, unreal.AnimPoseSpaces.WORLD) for n in [a, b]]
                points = [[t.translation.x, t.translation.y, t.translation.z] for t in transforms]
                assert all(math.isfinite(v) for p in points for v in p)
                frame.append(math.dist(*points))
            lengths.append(frame)
        variation = max(max(f[i] for f in lengths)-min(f[i] for f in lengths) for i in range(len(pairs)))
        assert variation < .01, (entry['label'], variation)
        REPORT['clips'].append({'label': entry['label'], 'samples': 21, 'max_limb_length_variation_cm': variation})
    world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_MotionTest')
    assert world
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    bases, hulls = [], []
    for actor in actors:
        if not isinstance(actor, unreal.SkeletalMeshActor):
            continue
        component = actor.skeletal_mesh_component
        leader = component.get_editor_property('leader_pose_component')
        if actor.get_actor_label().endswith(' Outline'):
            assert leader, 'Persisted outline lost its leader: ' + actor.get_actor_label()
            assert leader.get_outer().get_actor_label() == actor.get_actor_label().removesuffix(' Outline')
            hulls.append(actor)
        else:
            data = component.get_editor_property('animation_data')
            assert data.anim_to_play and data.saved_playing and data.saved_looping
            assert component.get_editor_property('skeletal_mesh') == target
            assert data.anim_to_play.get_path_name().split('.')[0] in [c['asset'] for c in audit['clips']]
            assert abs(data.saved_play_rate - 1) < .0001, actor.get_actor_label()
            bases.append(actor)
            REPORT['actors'].append({'label': actor.get_actor_label(), 'animation': data.anim_to_play.get_path_name(),
                                      'looping': data.saved_looping, 'rate': data.saved_play_rate})
    assert len(bases) == 6 and len(hulls) == 6
    camera = next(a for a in actors if isinstance(a, unreal.CameraActor))
    rot = camera.get_actor_rotation()
    assert abs(rot.pitch+55) < .001 and abs(rot.yaw+45) < .001 and abs(rot.roll) < .001, str(rot)
    assert camera.get_auto_activate_player_index() == 0
    REPORT.update(status='PASS', source_hashes_unchanged=True, outline_leaders_persisted=len(hulls),
                  map_camera_rotation={'pitch': rot.pitch, 'yaw': rot.yaw, 'roll': rot.roll},
                  notes='Structural/numerical test, not a gameplay or full-frame skinning quality test')


try:
    main()
except Exception:
    REPORT.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    path.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
if REPORT['status'] != 'PASS':
    raise RuntimeError('PG Toon motion validation failed: ' + str(path))
unreal.log('PG Toon motion validation PASS')
