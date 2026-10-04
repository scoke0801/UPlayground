"""Read-only fresh-process checks for all persisted Bokusei Katana clips/maps."""
import hashlib
import json
import math
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
AUDIT = json.loads((ROOT/'Saved/BokuseiMotionLibrary/configure.json').read_text(encoding='utf-8'))
REPORT = dict(status='RUNNING',clips=[],pages=[])
POSE = unreal.AnimPoseExtensions
OPTIONS = unreal.AnimPoseEvaluationOptions()
PAIRS = [('UpperArm_L','LowerArm_L'),('LowerArm_L','Hand_L'),('UpperArm_R','LowerArm_R'),('LowerArm_R','Hand_R'),
         ('UpperLeg_L','LowerLeg_L'),('LowerLeg_L','Foot_L'),('UpperLeg_R','LowerLeg_R'),('LowerLeg_R','Foot_R')]


def point(pose,bone):
    p=POSE.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD).translation
    return [p.x,p.y,p.z]


def main():
    assert AUDIT['status']=='PASS' and len(AUDIT['clips'])==60
    for file,expected in AUDIT['source_hashes'].items():
        assert hashlib.sha256(Path(file).read_bytes()).hexdigest()==expected,file
    target=unreal.load_asset('/Game/Art/ToonTest/Bokusei/SK_Bokusei_ToonTest')
    source=unreal.load_asset('/Game/Art/AnimationTests/AnimeKatana/SK_PGAnimeKatana')
    source_skeleton=source.get_editor_property('skeleton')
    assert isinstance(source_skeleton,unreal.Skeleton)
    for entry in AUDIT['clips']:
        clip=unreal.load_asset(entry['asset'])
        raw=unreal.load_asset(entry['source'])
        assert raw.get_editor_property('skeleton')==source_skeleton,entry['source']
        assert isinstance(clip,unreal.AnimSequence)
        assert clip.get_editor_property('skeleton')==target.get_editor_property('skeleton')
        assert abs(clip.get_play_length()-entry['duration'])<.001
        is_root=entry['group']=='Root_Motion'
        assert bool(clip.get_editor_property('enable_root_motion'))==is_root
        lengths=[]
        roots=[]
        for i in range(31):
            pose=clip.get_anim_pose_at_time(clip.get_play_length()*i/30,OPTIONS)
            roots.append(point(pose,'Armature'))
            lengths.append([math.dist(point(pose,a),point(pose,b)) for a,b in PAIRS])
        assert all(math.isfinite(x) for row in lengths+roots for x in row)
        variation=max(max(row[i] for row in lengths)-min(row[i] for row in lengths) for i in range(len(PAIRS)))
        assert variation<.01,(entry['file'],variation)
        row=dict(asset=entry['asset'],max_limb_variation_cm=variation)
        if is_root:
            assert clip.get_editor_property('root_motion_root_lock')==unreal.RootMotionRootLock.ANIM_FIRST_FRAME
            error=max(math.dist(a,b) for a,b in zip(roots,entry['root_positions']))
            assert error<.01,(entry['file'],'Saved root trajectory changed',error)
            row['persisted_root_trajectory_max_error_cm']=error
            row['root_travel_cm']=max(math.dist(p,roots[0]) for p in roots)
        REPORT['clips'].append(row)
    total=0
    for page in AUDIT['pages']:
        world=unreal.EditorLoadingAndSavingUtils.load_map(page['map'])
        assert world,page['map']
        actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        bases=[]
        hulls=[]
        expected={entry['asset'] for entry in page['clips']}
        actual=set()
        for actor in actors:
            if not isinstance(actor,unreal.SkeletalMeshActor):
                continue
            comp=actor.skeletal_mesh_component
            if actor.get_actor_label().endswith(' Outline'):
                leader=comp.get_editor_property('leader_pose_component')
                assert leader and leader.get_outer().get_actor_label()==actor.get_actor_label().removesuffix(' Outline')
                hulls.append(actor)
            else:
                data=comp.get_editor_property('animation_data')
                assert data.anim_to_play and data.saved_playing and data.saved_looping
                assert abs(data.saved_play_rate-1)<.0001
                assert comp.get_editor_property('skeletal_mesh')==target
                actual.add(data.anim_to_play.get_path_name().split('.')[0])
                bases.append(actor)
        assert actual==expected and len(bases)==len(hulls)==len(expected),(page['name'],actual)
        camera=next(a for a in actors if isinstance(a,unreal.CameraActor))
        assert camera.get_auto_activate_player_index()==0
        total+=len(bases)
        REPORT['pages'].append(dict(map=page['map'],clips=len(bases),outline_leaders=len(hulls)))
    assert total==60 and len(REPORT['pages'])==11
    REPORT.update(status='PASS',protected_hashes_unchanged=True,total=total,
                  notes='Asset/deformation/root-trajectory/map validation; gameplay movement is not wired')


try:
    main()
except Exception:
    REPORT.update(status='FAIL',error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (ROOT/'Saved/BokuseiMotionLibrary/validation.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
if REPORT['status']!='PASS':
    raise RuntimeError(REPORT['error'])
unreal.log('Bokusei library persisted validation PASS')
