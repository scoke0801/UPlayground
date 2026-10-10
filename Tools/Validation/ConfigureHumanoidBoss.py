"""Enemy-only motion retarget/data migration. Use RunHumanoidBoss.py for rollback/reload."""
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
from PlayableCharacterTransaction import Transaction, write_json
SPEC=json.loads((ROOT/'Tools/Validation/Data/HumanoidBoss.json').read_text(encoding='utf-8'))
MANIFEST=json.loads((ROOT/'Docs/Design/HumanoidBoss/motion-manifest.json').read_text(encoding='utf-8'))
DEST='/Game/DataCenter/HumanoidBoss'
EAL=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
TABLES={'enemies':'/Game/DataCenter/DataTables/Actor/DT_Enemy','skills':'/Game/DataCenter/DataTables/Skill/DT_Skill',
        'stats':'/Game/DataCenter/DataTables/Actor/DT_CharacterStat','deaths':'/Game/DataCenter/DataTables/Path/DT_Death',
        'stages':'/Game/DataCenter/DataTables/Stage/DT_StageData'}
RUN=Path(os.environ.get('PG_HUMANOID_RUN',str(ROOT/'Saved/HumanoidBoss')))
TX=None
MOTIONS=dict(SPEC['motions'],Walk='Frank_RPG_Katana_8Way_Walk_F',Run='Frank_RPG_Katana_8Way_Run_F')
ENTRIES={key:next(m for m in MANIFEST['motions'] if m['label']==label) for key,label in MOTIONS.items()}
SOURCE_MESHES={'AnimeKatana':'/Game/Art/AnimationTests/AnimeKatana/SK_PGAnimeKatana',
    'GrruzamSword':'/Game/Art/AnimationTests/GrruzamSword/Setup/Grruzam/SK_PGGrruzam',
    'FrankSlash':'/Game/Art/AnimationTests/FrankSlash/Setup/FrankKatana/SK_PGFrankKatana'}

def rows(path):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))

def save(asset):
    TX.mark_written(asset.get_path_name().split('.')[0])
    assert EAL.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()

def own(name,cls,factory):
    path=DEST+'/'+name
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(name,DEST,cls,factory)

def duplicate(source,name):
    path=DEST+'/'+name
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else EAL.duplicate_asset(source,path)

def ref(asset):
    path=asset.get_path_name().split('.')[0]
    return dict(AssetPath=dict(PackageName=path,AssetName=path.rsplit('/',1)[1]),SubPathString='')

def rig(name,mesh):
    result=own(name,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
    ctl=unreal.IKRigController.get_controller(result)
    assert ctl.set_skeletal_mesh(mesh)
    for c in list(ctl.get_retarget_chains()): ctl.remove_retarget_chain(c.chain_name)
    assert ctl.set_retarget_root('pelvis')
    comp=unreal.SkeletalMeshComponent();comp.set_skeletal_mesh_asset(mesh)
    bones={str(comp.get_bone_name(i)) for i in range(comp.get_num_bones())}
    chains=[('Spine','spine_01','spine_05' if 'spine_05' in bones else 'spine_03'),
            ('Neck','neck_01','neck_02' if 'neck_02' in bones else 'neck_01'),('Head','head','head')]
    for side in ('l','r'):
        chains.extend([(f'Shoulder_{side}',f'clavicle_{side}',f'clavicle_{side}'),
                       (f'Arm_{side}',f'upperarm_{side}',f'hand_{side}'),(f'Leg_{side}',f'thigh_{side}',f'foot_{side}'),
                       (f'Toe_{side}',f'ball_{side}',f'ball_{side}')])
        for finger in ('thumb','index','middle','ring','pinky'):
            if f'{finger}_01_{side}' in bones: chains.append((f'{finger}_{side}',f'{finger}_01_{side}',f'{finger}_03_{side}'))
    for chain,start,end in chains: assert str(ctl.add_retarget_chain(chain,start,end,'None'))==chain
    save(result)
    return result,chains

def retarget(target):
    tr,chains=rig('IK_Target',target)
    clips={}
    evidence=[]
    for pack,source_path in SOURCE_MESHES.items():
        source=unreal.load_asset(source_path);assert source
        sr,_=rig('IK_'+pack,source)
        rt=own('RTG_'+pack,unreal.IKRetargeter,unreal.IKRetargetFactory())
        ctl=unreal.IKRetargeterController.get_controller(rt)
        s,t=unreal.RetargetSourceOrTarget.SOURCE,unreal.RetargetSourceOrTarget.TARGET
        ctl.remove_all_ops();ctl.set_ik_rig(s,sr);ctl.set_ik_rig(t,tr)
        ctl.set_preview_mesh(s,source);ctl.set_preview_mesh(t,target)
        for op in ('IKRetargetPelvisMotionOp','IKRetargetFKChainsOp'):
            i=ctl.add_retarget_op('/Script/IKRig.'+op);assert i>=0;ctl.run_op_initial_setup(i)
        ctl.assign_ik_rig_to_all_ops(s,sr);ctl.assign_ik_rig_to_all_ops(t,tr)
        for name,_,_ in chains: assert ctl.set_source_chain(name,name)
        ctl.reset_retarget_pose('Default Pose',[],t)
        ctl.auto_align_all_bones(t,unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
        save(rt)
        selected={k:e for k,e in ENTRIES.items() if e['pack']==pack}
        inputs=unreal.IKRetargetBatchOperationInputs()
        inputs.assets_to_retarget=[EAL.find_asset_data(e['source']) for e in selected.values()]
        inputs.source_mesh=source;inputs.target_mesh=target;inputs.ik_retarget_asset=rt
        inputs.target_path=DEST;inputs.prefix='PGSwordSaint_'
        inputs.include_referenced_assets=False;inputs.overwrite_existing_files=True
        assert len(unreal.IKRetargetBatchOperation.run_batch_retarget(inputs))==len(selected)
        for key,e in selected.items():
            path=DEST+'/PGSwordSaint_'+e['source'].rsplit('/',1)[1]
            raw=unreal.load_asset(path);assert raw and raw.get_editor_property('skeleton')==target.get_editor_property('skeleton')
            clip=duplicate(path,'AS_'+key)
            # Replay newly retargeted tracks on rerun; never retain stale generated motion.
            controller=clip.get_editor_property('controller');controller.open_bracket('Sword saint in-place bake',should_transact=False)
            opts=unreal.AnimPoseEvaluationOptions()
            names=[str(b) for b in unreal.AnimationLibrary.get_animation_track_names(raw)]
            frames=round(raw.get_play_length()*30)
            poses=[raw.get_anim_pose_at_time(raw.get_play_length()*i/frames,opts) for i in range(frames+1)]
            try:
                controller.set_frame_rate(unreal.FrameRate(30,1),should_transact=False)
                controller.set_number_of_frames(unreal.FrameNumber(frames),should_transact=False)
                for bone in names:
                    transforms=[unreal.AnimPoseExtensions.get_bone_pose(p,bone,unreal.AnimPoseSpaces.LOCAL) for p in poses]
                    if bone=='root':transforms=[transforms[0]]*len(transforms)
                    assert controller.set_bone_track_keys(bone,[x.translation for x in transforms],[x.rotation for x in transforms],[x.scale3d for x in transforms],should_transact=False)
            finally:controller.close_bracket(should_transact=False)
            unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
            clip.set_editor_property('enable_root_motion',False);clip.set_editor_property('force_root_lock',True)
            save(raw);save(clip);clips[key]=clip
            positions=[]
            for i,p in enumerate(poses):
                x=unreal.AnimPoseExtensions.get_bone_pose(p,'hand_r',unreal.AnimPoseSpaces.WORLD).translation
                assert all(math.isfinite(v) for v in (x.x,x.y,x.z))
                positions.append([x.x,x.y,x.z])
            evidence.append(dict(key=key,source=e['source'],asset=clip.get_path_name(),duration=clip.get_play_length(),fps=30,
                                 configured_contact_fraction=SPEC['contact_fractions'].get(key),hand_positions=positions))
    write_json(RUN/'motion-samples.json',evidence)
    return clips

def montage(key,clip):
    factory=unreal.AnimMontageFactory();factory.set_editor_property('target_skeleton',clip.get_editor_property('skeleton'))
    factory.set_editor_property('source_animation',clip)
    result=own('AM_'+key,unreal.AnimMontage,factory)
    tracks=list(result.get_editor_property('slot_anim_tracks'))
    track=tracks[0].get_editor_property('anim_track');segments=list(track.get_editor_property('anim_segments'))
    assert len(tracks)==1 and len(segments)==1
    segments[0].set_editor_property('anim_reference',clip);segments[0].set_editor_property('anim_end_time',clip.get_play_length())
    track.set_editor_property('anim_segments',segments);tracks[0].set_editor_property('anim_track',track)
    tracks[0].set_editor_property('slot_name','DefaultSlot');result.set_editor_property('slot_anim_tracks',tracks)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(result)
    for prop in ('blend_in','blend_out'):
        value=result.get_editor_property(prop);value.set_editor_property('blend_time',.12);result.set_editor_property(prop,value)
    save(result);return result

def contact(values,definition,montages):
    start,time,key,multiplier,radius,angle=values
    x=unreal.PGEnemyAttackContact()
    shape={'Sweep':unreal.PGAttackPattern.SWEEP,'Thrust':unreal.PGAttackPattern.THRUST,'RingBurst':unreal.PGAttackPattern.RING_BURST}[definition['pattern']]
    for prop,value in dict(motion_start=start,time=time,shape=shape,
        radius=radius,half_angle=angle,length=definition.get('length',520),half_width=definition.get('half_width',55),
        inner_radius=definition.get('inner_radius',260),damage_multiplier=multiplier,bHeavyImpact=key in ('Combo3','Counter','Ring'),
        montage=montages[key],start_fraction=0.,contact_fraction=SPEC['contact_fractions'][key],end_fraction=SPEC['end_fractions'][key]).items():
        x.set_editor_property(prop,value)
    return x

def apply():
    global TX
    before={k:rows(p) for k,p in TABLES.items()};data=copy.deepcopy(before)
    write_json(RUN/'before.json',before)
    packages=list(TABLES.values())+[DEST+'/'+n for n in ('IK_Target','BP_15401','DA_Appearance','BS_Locomotion','DA_Presentation')]
    packages += [DEST+'/'+prefix+pack for pack in SOURCE_MESHES for prefix in ('IK_','RTG_')]
    packages += [DEST+'/'+prefix+key for key in MOTIONS for prefix in ('AS_','AM_')]
    packages += [DEST+'/PGSwordSaint_'+e['source'].rsplit('/',1)[1] for e in ENTRIES.values()]
    packages += [DEST+'/DA_'+d['profile'] for d in SPEC['skills'] if 'profile' in d]
    TX=Transaction(ROOT,RUN);TX.prepare(packages,ROOT/'Tools/Validation/Data/HumanoidBoss.json')
    source_paths=[p for e in ENTRIES.values() for p in (e['source'],e['asset'])]+list(SOURCE_MESHES.values())+[SPEC['appearance_source']]
    hashes={p:hashlib.sha256((ROOT/'Content'/(p.removeprefix('/Game/')+'.uasset')).read_bytes()).hexdigest() for p in source_paths}
    enemies={r['EnemyID']:r for r in data['enemies']};skills={r['SkillID']:r for r in data['skills']}
    bp=duplicate(enemies[SPEC['body_source']]['ActorClass'].split('.')[0],'BP_15401')
    cdo=unreal.get_default_object(bp.generated_class())
    target=cdo.mesh.get_editor_property('skeletal_mesh_asset')
    clips=retarget(target);montages={k:montage(k,v) for k,v in clips.items()}
    for d in SPEC['skills']:
        row=copy.deepcopy(skills[15106])
        row.update(Name='SwordSaint_'+str(d['id']),SkillID=d['id'],Desc=d['name'],Pattern=d['pattern'],TelegraphDuration=d['windup'],
            AimTrackingSeconds=d['aim'],TelegraphRadius=d['radius'],SkillRange=d['range'],MinimumActivationRange=d.get('minimum_range',0),
            RecoveryDuration=d['recovery'],RecoveryDamageBonus=.25,SkillCoolTime=d['cooldown'],MinimumBossPhase=d.get('phase',1),
            HalfAngleDegrees=70,TravelDistance=d.get('length',520),TravelSpeed=d.get('speed',900),LineHalfWidth=d.get('half_width',55),
            InnerSafeRadius=d.get('inner_radius',260),LandingTelegraphSeconds=d.get('landing',.45),EnemyDamageMultiplier=d.get('multiplier',1),
            EnemyProfile='None',PlayerProfile='None',AttackPressureCost=3,SelectionWeight=1.,ChainSkillIdList=[],
            bSyncMontageToPattern=False,ElitePresentationMontage='None',bHeavyImpactFeedback=True)
        if 'profile' in d:
            factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.PGEnemyAttackProfile)
            profile=own('DA_'+d['profile'],unreal.PGEnemyAttackProfile,factory)
            primary=[contact(v,d,montages) for v in d['contacts']]
            profile.set_editor_property('contacts',primary)
            profile.set_editor_property('phase_two_contacts',primary+[contact(v,d,montages) for v in d['phase_two_contacts']] if 'phase_two_contacts' in d else [])
            profile.set_editor_property('guard_counter',d.get('guard',False))
            if d.get('guard'):
                for field,key in [('guard_start_montage','GuardStart'),('guard_hold_montage','GuardHold'),('guard_accept_montage','GuardAccept'),('guard_end_montage','GuardEnd')]:profile.set_editor_property(field,montages[key])
            save(profile);row['EnemyProfile']=profile.get_path_name()
            row['MontagePath']=ref(montages[d['contacts'][0][2]])
        else:
            m=montages[d['motion']]
            row.update(MontagePath=ref(m),ElitePresentationMontage=m.get_path_name(),bSyncMontageToPattern=True,
                       WindupMontageFraction=0.,ImpactMontageFraction=SPEC['contact_fractions'][d['motion']])
        skills[d['id']]=row
    appearance=duplicate(SPEC['appearance_source'],'DA_Appearance')
    source_appearance=unreal.load_asset(SPEC['appearance_source'])
    appearance.set_editor_property('id','EclipseSwordSaint');appearance.set_editor_property('display_name',SPEC['name'])
    transform=source_appearance.get_editor_property('mesh_transform');transform.scale3d=transform.scale3d*SPEC['display_scale'];appearance.set_editor_property('mesh_transform',transform)
    attachments=list(source_appearance.get_editor_property('attachments'))
    assert len(attachments)==1
    attachments[0].set_editor_property('mesh',unreal.load_asset('/Game/Art/P09Modular/Meshes/SM_P09_Weapon_Sword_005'))
    appearance.set_editor_property('attachments',attachments);save(appearance)
    factory=unreal.BlendSpaceFactory1D();factory.set_editor_property('target_skeleton',target.get_editor_property('skeleton'))
    blend=own('BS_Locomotion',unreal.BlendSpace1D,factory)
    axis=unreal.BlendParameter();axis.set_editor_property('display_name','Speed');axis.set_editor_property('min',0.);axis.set_editor_property('max',345.)
    blend.set_editor_property('blend_parameters',[axis,axis,axis])
    samples=[]
    for speed,key in ((0,'Idle'),(170,'Walk'),(345,'Run')):
        sample=unreal.BlendSample();sample.set_editor_property('animation',clips[key]);sample.set_editor_property('sample_value',unreal.Vector(speed,0,0));samples.append(sample)
    blend.set_editor_property('sample_data',samples);save(blend)
    cdo.set_editor_property('character_tid',SPEC['enemy_id']);cdo.set_editor_property('ai_controller_class',unreal.PGRoleAIController)
    cdo.appearance_component.set_editor_property('default_appearance',appearance)
    cdo.set_editor_property('creature_locomotion',blend);cdo.mesh.set_editor_property('anim_class',unreal.PGCreatureAnimInstance.static_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    presentation=duplicate(enemies[15103]['Presentation'].split('.')[0],'DA_Presentation')
    presentation.set_editor_property('armor',[]);save(presentation)
    row=copy.deepcopy(enemies[SPEC['baseline']]);row.update(Name='SwordSaint',EnemyID=SPEC['enemy_id'],EnemyName=SPEC['name'],
        ActorClass=bp.generated_class().get_path_name(),SkillIdList=[d['id'] for d in SPEC['skills']],PhaseTwoSkillSequence=[15405,15403,15401,15402],
        MinimumCombatWait=SPEC['minimum_combat_wait'],GuardHalfAngle=SPEC['guard_half_angle'],GuardReduction=SPEC['guard_reduction'],Presentation=presentation.get_path_name())
    row['PhaseTransitionText']='월식 각성 · 연참 강화와 원월 파동'
    enemies[SPEC['enemy_id']]=row;data['enemies']=list(enemies.values());data['skills']=list(skills.values())
    stat=copy.deepcopy(next(r for r in data['stats'] if r['CharacterID']==SPEC['baseline']))
    stat.update(Name='SwordSaint',CharacterID=SPEC['enemy_id']);stat['Stats'].update(SPEC['stats'])
    data['stats']=[r for r in data['stats'] if r['CharacterID']!=SPEC['enemy_id']]+[stat]
    death=copy.deepcopy(next(r for r in data['deaths'] if r['ObjectTID']==SPEC['body_source']))
    death.update(Name='SwordSaint',ObjectTID=SPEC['enemy_id'],DeathMontagePath=[ref(montages['Death'])])
    data['deaths']=[r for r in data['deaths'] if r['ObjectTID']!=SPEC['enemy_id']]+[death]
    stage=next(r for r in data['stages'] if r['Id']==6);assert len(stage['Waves'])==1 and len(stage['Waves'][0]['MonsterSpawnInfos'])==1
    stage['Waves'][0]['MonsterSpawnInfos'][0]['MonsterId']=SPEC['enemy_id']
    for r in data['stats']:r['Stats']={k:v['Stats'] if isinstance(v,dict) else v for k,v in r['Stats'].items()}
    for key,path in TABLES.items():
        table=unreal.load_asset(path);assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(data[key],ensure_ascii=False));save(table)
    assert all(hashlib.sha256((ROOT/'Content'/(p.removeprefix('/Game/')+'.uasset')).read_bytes()).hexdigest()==h for p,h in hashes.items())
    write_json(RUN/'source-hashes.json',hashes)
    TX.data['status']='APPLIED_UNVERIFIED';TX.flush();validate()
    unreal.log('PGHumanoidBoss APPLY PASS')

def validate(validate_stage=True):
    data={k:rows(p) for k,p in TABLES.items()}
    enemy=next(r for r in data['enemies'] if r['EnemyID']==SPEC['enemy_id'])
    assert enemy['EnemyName']==SPEC['name'] and enemy['Role']=='Boss' and enemy['DropPoolId']=='Rogue.Boss'
    assert enemy['MinimumCombatWait']==SPEC['minimum_combat_wait']
    cdo=unreal.get_default_object(unreal.load_class(None,enemy['ActorClass']))
    skeleton=cdo.mesh.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
    assert cdo.get_editor_property('ai_controller_class')==unreal.PGRoleAIController.static_class()
    assert cdo.get_editor_property('creature_locomotion')
    for key in MOTIONS:
        clip=unreal.load_asset(DEST+'/AS_'+key);m=unreal.load_asset(DEST+'/AM_'+key)
        assert clip and m and m.get_editor_property('skeleton')==skeleton
        assert str(m.get_editor_property('slot_anim_tracks')[0].get_editor_property('slot_name'))=='DefaultSlot'
        assert not unreal.AnimationLibrary.get_animation_notify_events(clip) and not unreal.AnimationLibrary.get_animation_notify_events(m)
        assert not clip.get_editor_property('enable_root_motion') and clip.get_editor_property('force_root_lock')
    skills={r['SkillID']:r for r in data['skills']}
    for d in SPEC['skills']:
        row=skills[d['id']]
        for field,key in [('TelegraphDuration','windup'),('AimTrackingSeconds','aim'),('RecoveryDuration','recovery'),('SkillCoolTime','cooldown')]:
            assert abs(row[field]-d[key])<1.e-5,(d['id'],field)
        assert row['MinimumBossPhase']==d.get('phase',1)
        assert abs(row['EnemyDamageMultiplier']-d.get('multiplier',1))<1.e-5
        if 'profile' in d:
            profile=unreal.load_asset(row['EnemyProfile']);assert profile
            for prop,expected in [('contacts',d['contacts']),('phase_two_contacts',d['contacts']+d['phase_two_contacts'] if 'phase_two_contacts' in d else [])]:
                contacts=profile.get_editor_property(prop);assert len(contacts)==len(expected)
                for hit,values in zip(contacts,expected):
                    start,time,key,multiplier,radius,angle=values
                    for field,value in [('motion_start',start),('time',time),('damage_multiplier',multiplier),('radius',radius),('half_angle',angle),('contact_fraction',SPEC['contact_fractions'][key]),('end_fraction',SPEC['end_fractions'][key])]:
                        assert abs(hit.get_editor_property(field)-value)<1.e-5,(d['id'],field)
                    assert hit.get_editor_property('montage')==unreal.load_asset(DEST+'/AM_'+key)
    hashfile=RUN/'source-hashes.json'
    if hashfile.exists():
        for path,digest in json.loads(hashfile.read_text(encoding='utf-8')).items():
            assert hashlib.sha256((ROOT/'Content'/(path.removeprefix('/Game/')+'.uasset')).read_bytes()).hexdigest()==digest,path
    baseline=RUN/'before.json'
    if baseline.exists():
        before=json.loads(baseline.read_text(encoding='utf-8'))
        for key,id_key,changed in [('enemies','EnemyID',{15401}),('skills','SkillID',set(range(15401,15406))),('stats','CharacterID',{15401}),('deaths','ObjectTID',{15401})]:
            old=[r for r in before[key] if r[id_key] not in changed];new=[r for r in data[key] if r[id_key] not in changed]
            if key=='stats':
                for r in old+new:r['Stats']={k:v['Stats'] if isinstance(v,dict) else v for k,v in r['Stats'].items()}
            assert old==new,('Unrelated table rows changed',key)
        assert [r for r in before['stages'] if r['Id']!=6]==[r for r in data['stages'] if r['Id']!=6]
    stage=next(r for r in data['stages'] if r['Id']==6)
    if validate_stage:
        assert [(x['MonsterId'],x['SpawnCount']) for x in stage['Waves'][0]['MonsterSpawnInfos']]==[(15401,1)]
    unreal.log('PGHumanoidBoss VALIDATION PASS')

if __name__=='__main__':
    if '-PGHumanoidValidate' in unreal.SystemLibrary.get_command_line():validate()
    else:apply()
