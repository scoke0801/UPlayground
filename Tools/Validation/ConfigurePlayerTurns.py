"""Retarget sword turns and wire them before airborne/IK/combat layers. Transactional."""
import json
import math
import os
from pathlib import Path
import re
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
import ConfigureHumanoidLocomotion as locomotion
from PlayableCharacterTransaction import Transaction, write_json

SPEC_FILE = ROOT/'Tools/Validation/Data/PlayerTurns.json'
SPEC = json.loads(SPEC_FILE.read_text(encoding='utf-8'))
DEST = SPEC['destination']
RUN = Path(os.environ['PG_TURNS_RUN'])
BP = locomotion.SPEC['player_blueprint']
LABEL = 'PlayerTurn'
EAL = unreal.EditorAssetLibrary

def entries():
    paths = [str(e.package_name) for e in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/Art/AnimationTests/FrankSlash/Animations/Sword2')]
    result = {}
    for side in ('Left','Right'):
        for angle in (45,90,180):
            found = [p for p in paths if re.fullmatch('AS_PGFrank_Sword2_Turn_'+side+'_'+str(angle)+'_[a-z0-9]{10}',p.rsplit('/',1)[1])]
            assert len(found)==1,(side,angle,found)
            result[side+'_'+str(angle)] = found[0]
    return result

def root_yaw(clip, fraction):
    pose = clip.get_anim_pose_at_time(clip.get_play_length()*fraction, unreal.AnimPoseEvaluationOptions())
    return unreal.AnimPoseExtensions.get_bone_pose(pose,'root',unreal.AnimPoseSpaces.WORLD).rotation.rotator().yaw

def validate():
    data = unreal.load_asset(DEST+'/DA_PlayerLocomotion')
    assert data
    assert abs(data.get_editor_property('combat_strafe_seconds')-SPEC['combat_strafe_seconds'])<.001
    blend=unreal.load_asset(locomotion.SPEC['destination']+'/BS_PlayerSword')
    samples=list(blend.get_editor_property('sample_data'))
    assert len(samples)==27
    for angle,direction in locomotion.DIRECTIONS:
        for speed,gait in ((170,'Walk'),(600,'Run')):
            found=[s for s in samples if abs(s.get_editor_property('sample_value').x-angle)<.01 and abs(s.get_editor_property('sample_value').y-speed)<.01]
            assert len(found)==1,(angle,speed)
            clip=found[0].get_editor_property('animation')
            assert '_8Way_'+gait+'_'+direction+'_' in clip.get_name(),clip.get_name()
            assert clip.get_editor_property('skeleton')==blend.get_editor_property('skeleton')
            assert unreal.PGHumanoidLocomotionTools.get_blend_sample_count(blend,found[0].get_editor_property('sample_value'))>0
    turns = list(data.get_editor_property('turns'))
    assert sorted(round(t.angle) for t in turns)==[-180,-90,-45,45,90,180]
    rows=[]
    for turn in turns:
        clip=turn.animation
        assert clip and clip.get_editor_property('force_root_lock') and not clip.get_editor_property('enable_root_motion')
        assert clip.get_editor_property('skeleton')==unreal.load_asset(locomotion.SPEC['player_mesh']).get_editor_property('skeleton')
        assert not unreal.AnimationLibrary.get_animation_notify_events(clip)
        values=list(turn.rotation_progress)
        assert len(values)==SPEC['rotation_samples'] and abs(values[0])<.01 and abs(values[-1]-1)<.02
        assert all(math.isfinite(v) and -.1<=v<=1.2 for v in values)
        # Locked root is essential: actor yaw consumes the original rotation exactly once.
        roots=[root_yaw(clip,f) for f in (0,.25,.5,.75,1)]
        assert max(roots)-min(roots)<.1,roots
        pelvis=[]
        for fraction in (0,1):
            pose=clip.get_anim_pose_at_time(clip.get_play_length()*fraction,unreal.AnimPoseEvaluationOptions())
            pelvis.append(unreal.AnimPoseExtensions.get_bone_pose(pose,'pelvis',unreal.AnimPoseSpaces.WORLD).rotation.rotator().yaw)
        assert abs((pelvis[-1]-pelvis[0]+180)%360-180)<15,(clip.get_name(),'double body rotation',pelvis)
        rows.append(dict(angle=turn.angle,clip=clip.get_path_name(),seconds=clip.get_play_length(),root_yaw=roots,pelvis_endpoints=pelvis))
    bp=unreal.load_asset(BP)
    for name in ('PGPlayerTurnEvaluator','PGPlayerTurnBlend','PGPlayerTurnInput0','PGPlayerTurnInput100','PGPlayerTurnInput200'):
        assert unreal.find_object(None,bp.get_path_name()+':AnimGraph.'+name),name
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    write_json(RUN/'validation.json',dict(status='PASS',turns=rows,combat_strafe_seconds=SPEC['combat_strafe_seconds'],directional_samples=len(samples)))
    unreal.log('PGPlayerTurns VALIDATION PASS')

def apply():
    source=entries()
    packages=[BP,DEST+'/DA_PlayerLocomotion']
    packages += [DEST+'/'+p+LABEL+s for p,s in [('IK_','_Source'),('IK_','_Target'),('RTG_','')]]
    packages += [DEST+'/'+LABEL+'_'+p.rsplit('/',1)[1] for p in source.values()]
    tx=Transaction(ROOT,RUN);tx.prepare(packages,SPEC_FILE)
    locomotion.TX=tx;locomotion.DEST=DEST
    clips=locomotion.retarget(LABEL,'Sword2',unreal.load_asset(locomotion.SPEC['player_mesh']),source)
    factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.PGPlayerLocomotionData)
    data=locomotion.own('DA_PlayerLocomotion',unreal.PGPlayerLocomotionData,factory)
    turns=[]
    for key,path in source.items():
        side,degrees=key.split('_');angle=int(degrees)*(-1 if side=='Left' else 1)
        original=unreal.load_asset(path)
        progress=[];previous=root_yaw(original,0);total=0.
        for i in range(SPEC['rotation_samples']):
            yaw=root_yaw(original,i/(SPEC['rotation_samples']-1))
            total+=(yaw-previous+180)%360-180;previous=yaw
            progress.append(total/angle)
        assert abs(progress[-1]-1)<.02,(key,progress[-1])
        assert unreal.PGHumanoidLocomotionTools.remove_retargeted_turn_yaw(clips[key],progress,float(angle))
        locomotion.save(clips[key])
        turn=unreal.PGPlayerTurnMotion()
        turn.set_editor_property('animation',clips[key]);turn.set_editor_property('angle',float(angle));turn.set_editor_property('rotation_progress',progress)
        turns.append(turn)
    data.set_editor_property('turns',turns)
    data.set_editor_property('allow_moving_turns',SPEC['allow_moving_turns'])
    data.set_editor_property('max_turn_seconds',float(SPEC['max_turn_seconds']))
    for field,key in [('combat_strafe_seconds','combat_strafe_seconds'),('turn_play_rate','play_rate'),('start_turn_angle','start_angle'),('moving_pivot_angle','moving_pivot_angle'),
                      ('stationary_speed','stationary_speed'),('movement_resume_fraction','movement_resume_fraction'),
                      ('blend_in_seconds','blend_in_seconds'),('blend_out_seconds','blend_out_seconds'),('retarget_cancel_angle','retarget_cancel_angle')]:
        data.set_editor_property(field,float(SPEC[key]))
    locomotion.save(data)
    bp=unreal.load_asset(BP)
    assert unreal.PGHumanoidLocomotionTools.configure_player_turn_locomotion(bp,clips['Right_90'])
    locomotion.save(bp)
    tx.data['status']='APPLIED_UNVERIFIED';tx.flush()
    validate()
    unreal.log('PGPlayerTurns APPLY PASS')

if __name__=='__main__':
    validate() if '-PGPlayerTurnsValidate' in unreal.SystemLibrary.get_command_line() else apply()
