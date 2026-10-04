"""Fresh-process verification of every imported clip plus representative galleries."""
import collections
import hashlib
import json
import math
import os
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/BokuseiPacks'
MANIFEST=json.loads((ROOT/'Tools/Art/ToonTest/Bokusei/three_packs_manifest.json').read_text(encoding='utf-8'))
AUDIT=json.loads((OUT/'configure.json').read_text(encoding='utf-8'))
SETUP=json.loads((OUT/'setup.json').read_text(encoding='utf-8'))
MAPS_ONLY=os.environ.get('PG_BOKUSEI_PACK_MAPS_ONLY')=='1'
REPORT=dict(status='RUNNING',mode='maps' if MAPS_ONLY else 'full',clips=[],pages=[],source_diagnostics=[])
OPTIONS=unreal.AnimPoseEvaluationOptions()
POSE=unreal.AnimPoseExtensions
PAIRS=[('UpperArm_L','LowerArm_L'),('LowerArm_L','Hand_L'),('UpperArm_R','LowerArm_R'),('LowerArm_R','Hand_R'),
       ('UpperLeg_L','LowerLeg_L'),('LowerLeg_L','Foot_L'),('UpperLeg_R','LowerLeg_R'),('LowerLeg_R','Foot_R')]


def vec(v):return [v.x,v.y,v.z]


def main():
    assert AUDIT['status']=='PASS' and not AUDIT['failures']
    assert {c['id'] for c in AUDIT['clips']}=={c['id'] for c in MANIFEST['clips']},'Incomplete pack coverage'
    if MAPS_ONLY:
        previous=json.loads((OUT/'validation.json').read_text(encoding='utf-8'))
        assert previous['status']=='PASS' and {c['id'] for c in previous['clips']}=={c['id'] for c in AUDIT['clips']}
        for entry in AUDIT['clips']:
            file=ROOT/('Content/'+entry['asset'].removeprefix('/Game/')+'.uasset')
            assert hashlib.sha256(file.read_bytes()).hexdigest()==entry['asset_sha256'],entry['asset']
        REPORT['unchanged_validated_animation_files']=len(AUDIT['clips'])
    for mapping in [MANIFEST['source_hashes'],MANIFEST['meta_hashes'],json.loads((OUT/'protected.json').read_text())]:
        for file,expected in mapping.items():
            assert hashlib.sha256(Path(file).read_bytes()).hexdigest()==expected,file
    target=unreal.load_asset('/Game/Art/ToonTest/Bokusei/SK_Bokusei_ToonTest')
    temp=unreal.SkeletalMeshComponent()
    temp.set_skeletal_mesh_asset(target)
    all_bones=[str(temp.get_bone_name(i)) for i in range(temp.get_num_bones())]
    for index,entry in enumerate([] if MAPS_ONLY else AUDIT['clips']):
        clip=unreal.load_asset(entry['asset'])
        raw=unreal.load_asset(entry['source'])
        source=unreal.load_asset(SETUP['sources'][entry['rig']]['mesh'])
        assert clip.get_editor_property('skeleton')==target.get_editor_property('skeleton')
        assert raw.get_editor_property('skeleton')==source.get_editor_property('skeleton')
        assert abs(clip.get_play_length()-entry['duration'])<.001
        if entry.get('use_exported_range'):
            assert abs(clip.get_play_length()-1.0)<.001,'Verified Warrior FBX range must remain one second'
        root_motion=entry['mode']!='Body'
        assert bool(clip.get_editor_property('enable_root_motion'))==root_motion
        if root_motion:
            assert clip.get_editor_property('root_motion_root_lock')==unreal.RootMotionRootLock.ANIM_FIRST_FRAME
        lengths=[]
        roots=[]
        for i in range(9):
            pose=clip.get_anim_pose_at_time(clip.get_play_length()*i/8,OPTIONS)
            points={b:POSE.get_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD) for b in set(['Armature','Hips','Head']+[b for pair in PAIRS for b in pair])}
            assert all(math.isfinite(v) for t in points.values() for v in [*vec(t.translation),*vec(t.scale3d),t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w]),entry['asset']
            lengths.append([math.dist(vec(points[a].translation),vec(points[b].translation)) for a,b in PAIRS])
            roots.append(vec(points['Armature'].translation))
        variation=max(max(f[i] for f in lengths)-min(f[i] for f in lengths) for i in range(len(PAIRS)))
        assert variation<.05,(entry['asset'],variation)
        row=dict(id=entry['id'],asset=entry['asset'],max_limb_variation_cm=variation,root_motion=root_motion)
        if root_motion:
            error=max(math.dist(a,b) for a,b in zip(roots,entry['root_positions']))
            assert error<.1,(entry['asset'],error)
            row['persisted_root_error_cm']=error
        if entry['rig']=='FrankWhip':
            # Source FBX non-finite conversion diagnostics require broader pose
            # checks, including every target bone, rotation and scale.
            for i in range(31):
                pose=clip.get_anim_pose_at_time(clip.get_play_length()*i/30,OPTIONS)
                for bone in all_bones:
                    t=POSE.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD)
                    assert all(math.isfinite(v) for v in [*vec(t.translation),*vec(t.scale3d),t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w]),(entry['asset'],i,bone)
            row['all_target_bones_finite_samples']=31
        REPORT['clips'].append(row)
        if index%100==0:
            (OUT/'validation_progress.json').write_text(json.dumps(dict(checked=index+1,total=len(AUDIT['clips']))))
            unreal.SystemLibrary.collect_garbage()
    for page in AUDIT['pages']:
        assert unreal.EditorLoadingAndSavingUtils.load_map(page['map'])
        actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        expected={c['asset'] for c in page['clips']}
        actual=set();outlines=0
        for actor in actors:
            if not isinstance(actor,unreal.SkeletalMeshActor):continue
            comp=actor.skeletal_mesh_component
            if actor.get_actor_label().endswith(' Outline'):
                leader=comp.get_editor_property('leader_pose_component')
                assert leader and leader.get_outer().get_actor_label()==actor.get_actor_label().removesuffix(' Outline')
                outlines+=1
            else:
                data=comp.get_editor_property('animation_data')
                assert data.saved_playing and data.saved_looping and abs(data.saved_play_rate-1)<.001
                assert comp.get_editor_property('skeletal_mesh')==target
                actual.add(data.anim_to_play.get_path_name().split('.')[0])
        assert actual==expected and outlines==len(expected),(page['map'],actual)
        REPORT['pages'].append(dict(map=page['map'],clips=len(expected)))
    diagnostics=OUT/'import_diagnostics.json'
    if diagnostics.exists():REPORT['source_diagnostics'].append(json.loads(diagnostics.read_text()))
    REPORT.update(status='PASS',total=len(REPORT['clips']),packs=MANIFEST['packs'],protected_and_unity_sources_unchanged=True)


try:
    main()
except Exception:
    REPORT.update(status='FAIL',error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT/('validation_maps.json' if MAPS_ONLY else 'validation.json')).write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
if REPORT['status']!='PASS':raise RuntimeError(REPORT['error'])
