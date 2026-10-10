"""Dark Knight + BossyEnemy target retarget, combat registration, and reload checks."""
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import sys
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from PlayableCharacterTransaction import Transaction,write_json
from ConfigureMonsterVariations import TABLES,rows,ref
import ConfigureHumanoidBoss as motion_tools

SPEC= json.loads((ROOT/'Tools/Validation/Data/DarkKnightBoss.json').read_text(encoding='utf-8'))
DEST=SPEC['destination']; OUT=Path(os.environ.get('PG_DARK_KNIGHT_RUN',str(ROOT/'Saved/DarkKnightBoss/Validation')))
EAL=unreal.EditorAssetLibrary; TOOLS=unreal.AssetToolsHelpers.get_asset_tools(); TX=None


def own(name,cls,factory):
    return unreal.load_asset(DEST+'/'+name) if EAL.does_asset_exist(DEST+'/'+name) else TOOLS.create_asset(name,DEST,cls,factory)


def save(asset):
    TX.mark_written(asset.get_path_name().split('.')[0])
    assert EAL.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()


def duplicate(path,name):
    return unreal.load_asset(DEST+'/'+name) if EAL.does_asset_exist(DEST+'/'+name) else EAL.duplicate_asset(path,DEST+'/'+name)


def retarget():
    target=unreal.load_asset(SPEC['mesh']); source=unreal.load_asset(SPEC['source_mesh'])
    motion_tools.DEST=DEST; motion_tools.TX=TX
    sr,_=motion_tools.rig('IK_BossyEnemy',source)
    tr,chains=motion_tools.rig('IK_DarkKnight',target)
    rt=own('RTG_BossyEnemy_DarkKnight',unreal.IKRetargeter,unreal.IKRetargetFactory())
    ctl=unreal.IKRetargeterController.get_controller(rt)
    s,t=unreal.RetargetSourceOrTarget.SOURCE,unreal.RetargetSourceOrTarget.TARGET
    ctl.remove_all_ops();ctl.set_ik_rig(s,sr);ctl.set_ik_rig(t,tr)
    ctl.set_preview_mesh(s,source);ctl.set_preview_mesh(t,target)
    for op in ('IKRetargetPelvisMotionOp','IKRetargetFKChainsOp'):
        index=ctl.add_retarget_op('/Script/IKRig.'+op);assert index>=0;ctl.run_op_initial_setup(index)
    ctl.assign_ik_rig_to_all_ops(s,sr);ctl.assign_ik_rig_to_all_ops(t,tr)
    for name,_,_ in chains: assert ctl.set_source_chain(name,name)
    ctl.reset_retarget_pose('Default Pose',[],t)
    ctl.auto_align_all_bones(t,unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    save(rt)
    inputs=unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget=[EAL.find_asset_data(SPEC['source_root']+v) for v in SPEC['motions'].values()]
    inputs.source_mesh=source;inputs.target_mesh=target;inputs.ik_retarget_asset=rt
    inputs.target_path=DEST;inputs.prefix='PGDarkKnight_';inputs.include_referenced_assets=False;inputs.overwrite_existing_files=True
    assert len(unreal.IKRetargetBatchOperation.run_batch_retarget(inputs))==len(SPEC['motions'])
    clips={};samples={}
    for key,value in SPEC['motions'].items():
        raw=unreal.load_asset(DEST+'/PGDarkKnight_'+value.rsplit('/',1)[-1]);assert raw
        clip=duplicate(raw.get_path_name(),'AS_'+key)
        controller=clip.get_editor_property('controller');controller.open_bracket('Dark Knight in-place bake',should_transact=False)
        opts=unreal.AnimPoseEvaluationOptions()
        frames=unreal.AnimationLibrary.get_num_frames(raw)
        poses=[raw.get_anim_pose_at_time(raw.get_play_length()*i/frames,opts) for i in range(frames+1)]
        names=[str(n) for n in unreal.AnimationLibrary.get_animation_track_names(raw)]
        try:
            controller.set_number_of_frames(unreal.FrameNumber(frames),should_transact=False)
            for bone in names:
                transforms=[unreal.AnimPoseExtensions.get_bone_pose(p,bone,unreal.AnimPoseSpaces.LOCAL) for p in poses]
                if bone=='root':transforms=[transforms[0]]*len(transforms)
                assert controller.set_bone_track_keys(bone,[x.translation for x in transforms],[x.rotation for x in transforms],[x.scale3d for x in transforms],should_transact=False)
        finally:controller.close_bracket(should_transact=False)
        unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
        clip.set_editor_property('enable_root_motion',False);clip.set_editor_property('force_root_lock',True)
        save(raw);save(clip);clips[key]=clip
        points=[]
        for i,p in enumerate(poses):
            row={'time':raw.get_play_length()*i/frames}
            for bone in ('hand_r','hand_l','foot_r','foot_l','head'):
                v=unreal.AnimPoseExtensions.get_bone_pose(p,bone,unreal.AnimPoseSpaces.WORLD).translation
                assert all(math.isfinite(n) for n in (v.x,v.y,v.z))
                row[bone]=[v.x,v.y,v.z]
            points.append(row)
        samples[key]=points
    clips['Death']=duplicate(SPEC['death'],'AS_Death')
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(clips['Death']);save(clips['Death'])
    write_json(OUT/'motion-samples.json',samples)
    return target,clips,{key:motion_tools.montage(key,clip) for key,clip in clips.items()}


def apply():
    global TX
    before={k:rows(v) for k,v in TABLES.items()};data=copy.deepcopy(before)
    write_json(OUT/'before.json',before)
    eid=SPEC['enemy_id']; ids={s['id'] for s in SPEC['skills']}
    for key,id_key,selected in [('enemies','EnemyID',{eid}),('skills','SkillID',ids),('stats','CharacterID',{eid}),('deaths','ObjectTID',{eid})]:
        assert all(r['Name'].startswith('DarkKnight') for r in before[key] if r[id_key] in selected),('ID collision',key)
    names=['IK_BossyEnemy','IK_DarkKnight','RTG_BossyEnemy_DarkKnight','BP_15601','DA_StartUp','GA_Hit','BS_Locomotion','DA_Presentation']
    names += [p+k for k in list(SPEC['motions'])+['Death'] for p in ('AS_','AM_')]
    names += ['PGDarkKnight_'+v.rsplit('/',1)[-1] for v in SPEC['motions'].values()]
    names += ['DA_Skill_'+str(s['id']) for s in SPEC['skills']]
    TX=Transaction(ROOT,OUT);TX.prepare([v for k,v in TABLES.items() if k!='stages']+[DEST+'/'+n for n in names],ROOT/'Tools/Validation/Data/DarkKnightBoss.json')
    sources=[SPEC['mesh'],SPEC['sword'],SPEC['source_mesh'],SPEC['death']]+[SPEC['source_root']+v for v in SPEC['motions'].values()]
    hashes={p:hashlib.sha256((ROOT/'Content'/(p.removeprefix('/Game/')+'.uasset')).read_bytes()).hexdigest() for p in sources}
    if '-PGDarkKnightReuseMotions' in unreal.SystemLibrary.get_command_line():
        target=unreal.load_asset(SPEC['mesh'])
        clips={k:unreal.load_asset(DEST+'/AS_'+k) for k in list(SPEC['motions'])+['Death']}
        montages={k:unreal.load_asset(DEST+'/AM_'+k) for k in clips}
        assert all(clips.values()) and all(montages.values())
    else:target,clips,montages=retarget()
    enemies={r['EnemyID']:r for r in data['enemies']};skills={r['SkillID']:r for r in data['skills']}
    for d in SPEC['skills']:
        factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.PGEnemyAttackProfile)
        profile=own('DA_Skill_'+str(d['id']),unreal.PGEnemyAttackProfile,factory)
        contacts=[]
        for c in d['contacts']:
            hit=unreal.PGEnemyAttackContact()
            values=dict(motion_start=c['start'],time=c['time'],shape=unreal.PGAttackPattern.SWEEP,
                radius=c['radius'],half_angle=c['half_angle'],damage_multiplier=c['damage'],bHeavyImpact=True,
                montage=montages[c['motion']],start_fraction=c['pose'][0],contact_fraction=c['pose'][1],end_fraction=c['pose'][2])
            for k,v in values.items():hit.set_editor_property(k,v)
            contacts.append(hit)
        profile.set_editor_property('contacts',contacts);profile.set_editor_property('phase_two_contacts',[])
        profile.set_editor_property('bGuardCounter',False);save(profile)
        row=copy.deepcopy(skills[15401]);row.update(Name='DarkKnight_'+str(d['id']),SkillID=d['id'],Desc=d['name'],
            Pattern='Sweep',SkillRange=d['range'],MinimumActivationRange=0,SkillCoolTime=d['cooldown'],MinimumBossPhase=d['phase'],
            TelegraphDuration=d['contacts'][0]['time'],AimTrackingSeconds=.25,RecoveryDuration=d['recovery'],RecoveryDamageBonus=.3,
            TelegraphRadius=d['contacts'][0]['radius'],HalfAngleDegrees=d['contacts'][0]['half_angle'],
            EnemyProfile=profile.get_path_name(),PlayerProfile='None',MontagePath=ref(montages[d['contacts'][0]['motion']].get_path_name()),
            ElitePresentationMontage='None',bSyncMontageToPattern=False,AttackPressureCost=3,SelectionWeight=1.,ChainSkillIdList=[])
        skills[d['id']]=row
    startup=duplicate('/Game/DataCenter/MonsterVariations/DA_GolemStartUp','DA_StartUp')
    startup.set_editor_property('ActivateOnGivenAbilities',[])
    reactive=[]
    for ability in startup.get_editor_property('ReactiveAbilities'):
        if isinstance(unreal.get_default_object(ability),unreal.PGAbilityHitReact):
            hit=duplicate(ability.get_path_name().split('.')[0],'GA_Hit')
            unreal.get_default_object(hit.generated_class()).set_editor_property('montage_paths',[unreal.SoftObjectPath(montages['Hit'+key].get_path_name()) for key in ('Front','Back','Left','Right')])
            unreal.BlueprintEditorLibrary.compile_blueprint(hit);save(hit);reactive.append(hit.generated_class())
        else:reactive.append(ability)
    startup.set_editor_property('ReactiveAbilities',reactive);save(startup)
    bf=unreal.BlendSpaceFactory1D();bf.set_editor_property('target_skeleton',target.get_editor_property('skeleton'))
    blend=own('BS_Locomotion',unreal.BlendSpace1D,bf)
    axis=unreal.BlendParameter();axis.set_editor_property('display_name','Speed');axis.set_editor_property('max',SPEC['movement_speed'])
    blend.set_editor_property('blend_parameters',[axis,axis,axis]);samples=[]
    for speed,key in ((0,'Idle'),(150,'Walk'),(SPEC['movement_speed'],'Run')):
        sample=unreal.BlendSample();sample.set_editor_property('animation',clips[key]);sample.set_editor_property('sample_value',unreal.Vector(speed,0,0));samples.append(sample)
    blend.set_editor_property('sample_data',samples);unreal.PGHumanoidLocomotionTools.rebuild_blend_space(blend);save(blend)
    factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.PGCharacterEnemy)
    bp=own('BP_15601',unreal.Blueprint,factory);cdo=unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('character_tid',eid);cdo.set_editor_property('ai_controller_class',unreal.PGRoleAIController)
    cdo.set_editor_property('character_start_up_data',startup);cdo.set_editor_property('creature_locomotion',blend)
    cdo.mesh.set_skeletal_mesh_asset(target);cdo.mesh.set_editor_property('anim_class',unreal.PGCreatureAnimInstance.static_class())
    cdo.mesh.set_editor_property('animation_mode',unreal.AnimationMode.ANIMATION_BLUEPRINT)
    cdo.mesh.set_editor_property('relative_scale3d',unreal.Vector(SPEC['scale'],SPEC['scale'],SPEC['scale']))
    cdo.capsule_component.set_capsule_size(58,125)
    cdo.mesh.set_editor_property('relative_location',unreal.Vector(0,0,-125))
    cdo.mesh.set_editor_property('relative_rotation',unreal.Rotator(pitch=0,yaw=-90,roll=0))
    cdo.mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    presentation=duplicate(enemies[15401]['Presentation'].split('.')[0],'DA_Presentation')
    pose=clips['Idle'].get_anim_pose_at_time(0,unreal.AnimPoseEvaluationOptions())
    hand=unreal.AnimPoseExtensions.get_bone_pose(pose,'hand_r',unreal.AnimPoseSpaces.WORLD)
    finger=unreal.AnimPoseExtensions.get_bone_pose(pose,'middle_01_r',unreal.AnimPoseSpaces.WORLD)
    palm=hand.translation+(finger.translation-hand.translation)*.65
    rotation=unreal.MathLibrary.make_rot_from_y(unreal.Vector(*SPEC['sword_idle_direction']))
    basis=unreal.Transform(rotation=rotation)
    grip=unreal.Vector(*SPEC['sword_grip_point'])
    location=palm-unreal.MathLibrary.transform_location(basis,grip)
    relative=unreal.MathLibrary.make_relative_transform(unreal.Transform(location=location,rotation=rotation),hand)
    piece=unreal.PGEnemyArmorPiece()
    for field,value in dict(name='PGDarkKnightSword',mesh=unreal.load_asset(SPEC['sword']),socket='hand_r',guard_transform=relative,recovery_transform=relative).items():
        piece.set_editor_property(field,value)
    presentation.set_editor_property('armor',[piece]);save(presentation)
    write_json(OUT/'sword-grip.json',dict(relative=relative.export_text(),palm=str(palm),grip_point=SPEC['sword_grip_point']))
    base=unreal.get_default_object(unreal.load_class(None,enemies[15401]['ActorClass']))
    cdo.set_editor_property('feedback_data',base.get_editor_property('feedback_data'))
    label=cdo.get_editor_property('enemy_nameplate_widget_component');source_label=base.get_editor_property('enemy_nameplate_widget_component')
    for field in ('widget_class','space','draw_size','draw_at_desired_size'):label.set_editor_property(field,source_label.get_editor_property(field))
    label.set_editor_property('relative_location',unreal.Vector(0,0,155))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    enemy=copy.deepcopy(enemies[15401]);enemy.update(Name='DarkKnight',EnemyID=eid,EnemyName=SPEC['name'],
        SkillIdList=[s['id'] for s in SPEC['skills']],ActorClass=bp.generated_class().get_path_name(),Presentation=presentation.get_path_name(),
        MinimumCombatWait=SPEC['minimum_wait'],TurnSpeed=150,PhaseTwoSkillSequence=SPEC['phase_two_sequence'],
        PhaseTransitionText='흑철 격노 · 파쇄 연격 개방',PhaseTwoHealthRatio=.5,PhaseTransitionSeconds=1.5)
    enemies[eid]=enemy;data['enemies']=list(enemies.values());data['skills']=list(skills.values())
    stat=copy.deepcopy(next(r for r in data['stats'] if r['CharacterID']==15401))
    stat.update(Name='DarkKnight',CharacterID=eid)
    stat['Stats']={k:v['Stats'] if isinstance(v,dict) else v for k,v in stat['Stats'].items()};stat['Stats']['MovementSpeed']=SPEC['movement_speed']
    data['stats']=[r for r in data['stats'] if r['CharacterID']!=eid]+[stat]
    death=copy.deepcopy(next(r for r in data['deaths'] if r['ObjectTID']==15401))
    death.update(Name='DarkKnight',ObjectTID=eid,DeathMontagePath=[ref(montages['Death'].get_path_name())])
    data['deaths']=[r for r in data['deaths'] if r['ObjectTID']!=eid]+[death]
    for k,path in TABLES.items():
        if k=='stages':continue
        if k=='stats':
            for r in data[k]:r['Stats']={n:v['Stats'] if isinstance(v,dict) else v for n,v in r['Stats'].items()}
        table=unreal.load_asset(path);assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(data[k],ensure_ascii=False));save(table)
    assert all(hashlib.sha256((ROOT/'Content'/(p.removeprefix('/Game/')+'.uasset')).read_bytes()).hexdigest()==h for p,h in hashes.items())
    write_json(OUT/'source-hashes.json',hashes);TX.data['status']='APPLIED_UNVERIFIED';TX.flush()
    validate();unreal.log('PGDarkKnight apply PASS')


def validate():
    data={k:rows(v) for k,v in TABLES.items()}
    enemy=next(r for r in data['enemies'] if r['EnemyID']==SPEC['enemy_id'])
    assert enemy['Role']=='Boss' and enemy['SkillIdList']==[s['id'] for s in SPEC['skills']]
    cdo=unreal.get_default_object(unreal.load_class(None,enemy['ActorClass']))
    assert cdo.mesh.get_editor_property('skeletal_mesh_asset')==unreal.load_asset(SPEC['mesh'])
    rotation=cdo.mesh.get_editor_property('relative_rotation')
    assert abs(rotation.pitch)<.001 and abs(rotation.roll)<.001 and abs(rotation.yaw+90)<.001,rotation
    skeleton=cdo.mesh.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
    presentation=unreal.load_asset(enemy['Presentation'])
    armor=presentation.get_editor_property('armor');assert len(armor)==1
    assert str(armor[0].get_editor_property('socket'))=='hand_r'
    assert armor[0].get_editor_property('mesh')==unreal.load_asset(SPEC['sword'])
    for key in list(SPEC['motions'])+['Death']:
        clip=unreal.load_asset(DEST+'/AS_'+key);montage=unreal.load_asset(DEST+'/AM_'+key)
        assert clip and montage and clip.get_editor_property('skeleton')==skeleton and montage.get_editor_property('skeleton')==skeleton
        assert not unreal.AnimationLibrary.get_animation_notify_events(clip) and not unreal.AnimationLibrary.get_animation_notify_events(montage)
        assert clip.get_play_length()>0
    skills={r['SkillID']:r for r in data['skills']}
    for d in SPEC['skills']:
        row=skills[d['id']];profile=unreal.load_asset(row['EnemyProfile'])
        contacts=profile.get_editor_property('contacts');assert len(contacts)==len(d['contacts'])
        assert row['SkillCoolTime']==d['cooldown'] and row['MinimumBossPhase']==d['phase']
        for c,x in zip(d['contacts'],contacts):
            assert x.get_editor_property('montage')==unreal.load_asset(DEST+'/AM_'+c['motion'])
            for field,value in [('motion_start',c['start']),('time',c['time']),('radius',c['radius']),('half_angle',c['half_angle']),('damage_multiplier',c['damage']),('start_fraction',c['pose'][0]),('contact_fraction',c['pose'][1]),('end_fraction',c['pose'][2])]:
                assert abs(x.get_editor_property(field)-value)<1.e-5,(d['id'],field)
    if (OUT/'before.json').exists():
        before=json.loads((OUT/'before.json').read_text(encoding='utf-8'))
        for key,field,ids in [('enemies','EnemyID',{SPEC['enemy_id']}),('skills','SkillID',{s['id'] for s in SPEC['skills']}),('stats','CharacterID',{SPEC['enemy_id']}),('deaths','ObjectTID',{SPEC['enemy_id']})]:
            old=[r for r in before[key] if r[field] not in ids];new=[r for r in data[key] if r[field] not in ids]
            if key=='stats':
                for r in old+new:r['Stats']={n:v['Stats'] if isinstance(v,dict) else v for n,v in r['Stats'].items()}
            assert old==new,('Unrelated row changed',key)
        assert before['stages']==data['stages'],'Stage roster changed'
    if (OUT/'source-hashes.json').exists():
        for path,digest in json.loads((OUT/'source-hashes.json').read_text(encoding='utf-8')).items():
            assert hashlib.sha256((ROOT/'Content'/(path.removeprefix('/Game/')+'.uasset')).read_bytes()).hexdigest()==digest,path
    write_json(OUT/'validation.json',dict(status='PASS',enemy_id=SPEC['enemy_id'],skills=[s['id'] for s in SPEC['skills']],motions=len(SPEC['motions'])+1))
    unreal.log('PGDarkKnight validate PASS')


def apply_roster():
    before=rows(TABLES['stages']);after=copy.deepcopy(before)
    stage=next(r for r in after if r['Id']==SPEC['auto_spawn_stage'])
    assert stage['bIsBossStage'] and len(stage['Waves'])==1
    spawns=stage['Waves'][0]['MonsterSpawnInfos']
    assert len(spawns)==1 and spawns[0]['SpawnCount']==1
    assert spawns[0]['MonsterId'] in (15106,15401,SPEC['enemy_id'])
    validate()
    tx=Transaction(ROOT,OUT);tx.prepare([TABLES['stages']],ROOT/'Tools/Validation/Data/DarkKnightBoss.json')
    write_json(OUT/'stage-before.json',before)
    spawns[0]['MonsterId']=SPEC['enemy_id']
    table=unreal.load_asset(TABLES['stages'])
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(after,ensure_ascii=False))
    tx.mark_written(TABLES['stages']);assert EAL.save_loaded_asset(table,only_if_is_dirty=False)
    assert rows(TABLES['stages'])==after
    tx.data['status']='APPLIED_UNVERIFIED';tx.flush()
    write_json(OUT/'stage-after.json',after)
    unreal.log('PGDarkKnight roster PASS')


if __name__=='__main__':
    if '-PGDarkKnightRoster' in unreal.SystemLibrary.get_command_line():apply_roster()
    elif '-PGDarkKnightValidate' in unreal.SystemLibrary.get_command_line():validate()
    else:apply()
