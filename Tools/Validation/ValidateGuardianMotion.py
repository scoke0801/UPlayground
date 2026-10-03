"""Read-only validation of saved guardian animation bindings, grip, feet and slam poses."""
import math
import unreal

DATA = '/Game/DataCenter/Guardian/'
SOURCE = '/Game/ExternalAssets/Characters/Enemies/SkeletonEnemy/Animations/'


def distance(a, b):
    return math.sqrt((a.x-b.x)**2+(a.y-b.y)**2+(a.z-b.z)**2)


def validate_guardian_motion(enemies, skills):
    blueprint = unreal.load_asset(DATA+'ABP_PGGuardian')
    assert isinstance(blueprint,unreal.AnimBlueprint),'Missing dedicated guardian AnimBP'
    guardian_class = blueprint.generated_class()
    defaults = unreal.get_default_object(guardian_class)
    clips = {name:unreal.load_asset(DATA+'AS_PGGuardian'+name) for name in ['Idle','Walk','Run','Slam']}
    options = unreal.AnimPoseEvaluationOptions()

    def position(clip, bone, t):
        pose = clip.get_anim_pose_at_time(clip.get_play_length()*t, options)
        return unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD).translation

    for name,clip in clips.items():
        assert isinstance(clip,unreal.AnimSequence),name
        assert clip.get_editor_property('skeleton')==blueprint.get_editor_property('target_skeleton')
        assert not unreal.AnimationLibrary.get_animation_notify_events(clip),'Cosmetic sequence contains gameplay notifies: '+name
        assert distance(position(clip,'hand_l',0),position(clip,'hand_l',1))<1.,'Shield pose jumps at loop/recovery seam: '+name
        if name=='Slam':
            continue
        source = unreal.load_asset(SOURCE+'Anim_'+name+'_Sword')
        for index in range(9):
            t = index/8
            hand = position(clip,'hand_l',t)
            assert hand.y>18 and hand.z>98,'Shield arm was lost: '+name
            for foot in ['foot_l','foot_r']:
                assert distance(position(clip,foot,t),position(source,foot,t))<.5,'Source foot contacts changed: '+name

    for name in ['Default','Strafing']:
        blend = defaults.get_editor_property(name+'BlendSpace')
        assert blend and blend.get_path_name()==DATA+'BS_PGGuardian'+name+'.BS_PGGuardian'+name
        samples = blend.get_editor_property('sample_data')
        assert {s.get_editor_property('animation') for s in samples}=={clips[n] for n in ['Idle','Walk','Run']}
    for enemy_id,row in enemies.items():
        cdo = unreal.get_default_object(unreal.load_class(None,row['ActorClass']))
        if not isinstance(cdo,unreal.PGCharacterEnemy):
            continue
        anim_class = cdo.get_editor_property('mesh').get_editor_property('anim_class')
        assert (anim_class==guardian_class)==(enemy_id==15103),'Guardian animation isolation/binding invalid: '+str(enemy_id)

    skill = skills[15103]
    slam = clips['Slam']
    authored_impact = unreal.EditorAssetLibrary.get_metadata_tag(slam,'PGGuardianImpactFraction')
    assert authored_impact and abs(float(authored_impact)-skill['ImpactMontageFraction'])<.0001,'Rebake motion after changing impact fraction'
    raised = position(slam,'hand_r',.4)
    strike = position(slam,'hand_r',skill['ImpactMontageFraction'])
    assert raised.z>145 and strike.y>30 and strike.z<110 and raised.z-strike.z>40,'Slam anticipation/impact not distinct'
    montage = unreal.load_asset(skill['ElitePresentationMontage'])
    segments = [s for t in montage.get_editor_property('slot_anim_tracks') for s in t.get_editor_property('anim_track').get_editor_property('anim_segments')]
    assert len(segments)==1 and segments[0].get_editor_property('anim_reference')==slam,'Montage still references shared sword attack'
    segment = segments[0]
    # The inherited source clip has a playback scale of 0.8. Pattern sync
    # operates on montage time, so validate the effective duration, not raw clip time.
    rate = segment.get_editor_property('anim_play_rate')*slam.get_editor_property('rate_scale')
    assert rate>0 and segment.get_editor_property('looping_count')==1
    assert abs(segment.get_editor_property('start_pos'))<.001 and abs(segment.get_editor_property('anim_start_time'))<.001
    assert abs(segment.get_editor_property('anim_end_time')-slam.get_play_length())<.001
    assert abs(montage.get_play_length()-slam.get_play_length()/rate)<.001, (montage.get_play_length(),slam.get_play_length(),rate)
    unreal.log('PGGuardian Motion VALIDATION PASS clips=4 blendspaces=2 feet_preserved=true')
    return 1
