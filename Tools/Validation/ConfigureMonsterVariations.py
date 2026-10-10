"""Versioned monster loadouts and encounters. Run through RunMonsterVariations.py."""
import copy
import json
import math
import os
from pathlib import Path
import sys
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from MonsterVariationRoster import SPEC, IDS, P09_IDS, compose, validate_roster
from PlayableCharacterTransaction import Transaction
DEST='/Game/DataCenter/MonsterVariations'
BASE='/Game/Blueprints/Actor/NonPlayer/Enemy/'
P09='/Game/Art/P09Modular/Meshes/'
TABLES={'enemies':'/Game/DataCenter/DataTables/Actor/DT_Enemy',
        'stats':'/Game/DataCenter/DataTables/Actor/DT_CharacterStat',
        'skills':'/Game/DataCenter/DataTables/Skill/DT_Skill',
        'deaths':'/Game/DataCenter/DataTables/Path/DT_Death',
        'stages':'/Game/DataCenter/DataTables/Stage/DT_StageData'}
EAL=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
TX=None

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

def ref(path):
    path=path.split('.')[0]
    return dict(AssetPath=dict(PackageName=path,AssetName=path.rsplit('/',1)[1]),SubPathString='')

def montage(definition):
    sid=definition['id']
    if 'montage' in definition:
        source=unreal.load_asset(BASE+definition['montage'])
        tracks=list(source.get_editor_property('slot_anim_tracks'))
        segments=list(tracks[0].get_editor_property('anim_track').get_editor_property('anim_segments'))
        assert len(tracks)==1 and len(segments)==1
        source_sequence=segments[0].get_editor_property('anim_reference')
    else:
        source=None
        source_sequence=unreal.load_asset(definition['sequence'])
    assert source_sequence
    clip=duplicate(source_sequence.get_path_name(),'AS_'+str(sid))
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
    clip.set_editor_property('enable_root_motion',False)
    clip.set_editor_property('force_root_lock',True)
    save(clip)
    if source:
        result=duplicate(source.get_path_name(),'AM_'+str(sid))
        track=tracks[0].get_editor_property('anim_track')
        segments[0].set_editor_property('anim_reference',clip)
        track.set_editor_property('anim_segments',segments)
        tracks[0].set_editor_property('anim_track',track)
        result.set_editor_property('slot_anim_tracks',tracks)
    else:
        factory=unreal.AnimMontageFactory()
        factory.set_editor_property('target_skeleton',clip.get_editor_property('skeleton'))
        factory.set_editor_property('source_animation',clip)
        result=own('AM_'+str(sid),unreal.AnimMontage,factory)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(result)
    save(result)
    return result

def golem_anim():
    prefix='/Game/ExternalAssets/Characters/Fantasy_Pack/Animations/2Without_Weapon/'
    idle=unreal.load_asset(prefix+'Anim_Idle_Without_Weapon')
    factory=unreal.BlendSpaceFactory1D()
    factory.set_editor_property('target_skeleton',idle.get_editor_property('skeleton'))
    blend=own('BS_Golem',unreal.BlendSpace1D,factory)
    axis=unreal.BlendParameter()
    axis.set_editor_property('display_name','Speed')
    axis.set_editor_property('min',0.)
    axis.set_editor_property('max',300.)
    # Native editor API owns sample triangulation when inserting samples.
    blend.set_editor_property('blend_parameters',[axis,axis,axis])
    samples=[]
    for speed,name in [(0,'Idle'),(150,'Walk'),(300,'Run')]:
        sample=unreal.BlendSample()
        sample.set_editor_property('animation',unreal.load_asset(prefix+'Anim_'+name+'_Without_Weapon'))
        sample.set_editor_property('sample_value',unreal.Vector(speed,0,0))
        samples.append(sample)
    blend.set_editor_property('sample_data',samples)
    unreal.PGHumanoidLocomotionTools.rebuild_blend_space(blend)
    save(blend)
    # Use the native class directly: generated AnimBP graphs replace custom roots.
    return blend

def appearance(grade,sex,eid):
    source='/Game/DataCenter/Characters/DA_P09_'+sex
    asset=duplicate(source,'DA_'+str(eid))
    # Reset only this generated loadout from its calibrated base each time.
    source_asset=unreal.load_asset(source)
    for field in ('mesh','source_mesh','retargeter','mesh_transform','head_bone','head_forward_axis','head_right_axis','equipment_bones','reconstruct_scaled_translations'):
        asset.set_editor_property(field,source_asset.get_editor_property(field))
    asset.set_editor_property('id','P09_'+grade['grade']+'_'+sex)
    asset.set_editor_property('display_name',grade['title'])
    prefix='Fem' if sex=='Female' else 'Male'
    parts=[]
    for label in ['Head','Chest','Arm','Waist','Leg']:
        mesh=unreal.load_asset(P09+'SK_P09_'+prefix+'_Armor_'+grade['armor']+'_'+label)
        assert mesh
        part=unreal.PGAppearancePart()
        part.set_editor_property('mesh',mesh)
        part.set_editor_property('relative_transform',unreal.Transform())
        parts.append(part)
    # Preserve calibrated independent hair attachment.
    parts.extend(p for p in source_asset.get_editor_property('parts') if str(p.attach_bone)!='None')
    asset.set_editor_property('parts',parts)
    asset.set_editor_property('attachments',make_attachments(grade,sex))
    save(asset)
    return asset


def make_attachments(grade,sex):
    attachments=[]
    for kind,number,bone in [('Sword',grade['sword'],'hand_r'),('Shield',grade['shield'],'hand_l')]:
        if not number: continue
        attachment=unreal.PGAppearanceAttachment()
        attachment.set_editor_property('mesh',unreal.load_asset(P09+'SM_P09_Weapon_'+kind+'_'+number))
        definition=SPEC['attachments'][kind]
        profile=definition['profiles'][sex]
        attachment.set_editor_property('attach_bone',definition['bone'])
        # P09 hand bones carry the imported 100x scale; static gear is in cm.
        transform=unreal.Transform(location=unreal.Vector(*profile['translation']),scale=unreal.Vector(*profile['scale']))
        pitch,yaw,roll=profile['rotation']
        transform.rotation=unreal.Rotator(pitch=pitch,yaw=yaw,roll=roll).quaternion()
        attachment.set_editor_property('relative_transform',transform)
        fingers=[]
        for value in profile.get('fingers',[]):
            finger=unreal.PGAppearanceGripFinger()
            finger.bone=value['bone']
            pitch,yaw,roll=value['rotation']
            finger.reference_rotation_offset=unreal.Rotator(pitch=pitch,yaw=yaw,roll=roll)
            fingers.append(finger)
        attachment.set_editor_property('fingers',fingers)
        attachments.append(attachment)
    return attachments


def apply_grips():
    """Only the six appearances; never rewrite combat tables while tuning a grip."""
    global TX
    TX=Transaction(ROOT,Path(os.environ['PG_MONSTER_RUN']))
    TX.prepare([DEST+'/DA_'+str(i) for i in P09_IDS],ROOT/'Tools/Validation/Data/MonsterVariations.json')
    for grade in SPEC['p09_grades']:
        for sex,eid in zip(('Female','Male'),grade['ids']):
            asset=unreal.load_asset(DEST+'/DA_'+str(eid))
            assert asset
            asset.set_editor_property('attachments',make_attachments(grade,sex))
            save(asset)
    TX.data['status']='APPLIED_UNVERIFIED';TX.flush()
    validate_grips()
    unreal.log('PGMonsterGrips APPLY PASS')


def validate_grips():
    for grade in SPEC['p09_grades']:
        for sex,eid in zip(('Female','Male'),grade['ids']):
            asset=unreal.load_asset(DEST+'/DA_'+str(eid))
            actual=list(asset.get_editor_property('attachments'))
            expected=make_attachments(grade,sex)
            assert len(actual)==len(expected)
            component=unreal.SkeletalMeshComponent()
            component.set_skeletal_mesh_asset(asset.get_editor_property('mesh'))
            names={str(component.get_bone_name(i)).casefold() for i in range(component.get_num_bones())}
            for a,b in zip(actual,expected):
                assert a.export_text()==b.export_text(),(eid,a.export_text(),b.export_text())
                hand=str(a.attach_bone).casefold()
                assert hand in names
                assert len({str(f.bone).casefold() for f in a.fingers})==len(a.fingers)
                for finger in a.fingers:
                    bone=str(finger.bone).casefold()
                    assert bone in names and bone!=hand,(eid,bone)
                    parent=str(component.get_parent_bone(finger.bone)).casefold()
                    while parent not in ('none',hand):
                        parent=str(component.get_parent_bone(parent)).casefold()
                    assert parent==hand,(eid,'finger outside attachment hand',bone)
                    assert all(math.isfinite(getattr(finger.reference_rotation_offset,axis)) for axis in ('pitch','yaw','roll')),(eid,bone)
    unreal.log('PGMonsterGrips VALIDATION PASS')

def apply():
    global TX
    run=Path(os.environ['PG_MONSTER_RUN'])
    before={key:rows(path) for key,path in TABLES.items()}
    data=copy.deepcopy(before)
    by_enemy={r['EnemyID']:r for r in data['enemies']}
    by_skill={r['SkillID']:r for r in data['skills']}
    planned=list(TABLES.values())+[DEST+'/BS_Golem']
    planned += [DEST+'/'+prefix+str(d['id']) for d in SPEC['skills'] for prefix in ('AS_','AM_')]
    planned += [DEST+'/BP_'+str(i) for i in IDS]+[DEST+'/DA_'+str(i) for i in P09_IDS]
    # A dedicated compatible death clip for the golem.
    planned += [DEST+'/AS_15350',DEST+'/AM_15350',DEST+'/AS_15351',DEST+'/AM_15351',DEST+'/DA_GolemStartUp',DEST+'/GA_GolemHit']
    for definition in SPEC['creatures']:
        assert definition['source'] in by_enemy
    compose(data['stages'])  # Refuse unknown encounter budgets before any package writes.
    TX=Transaction(ROOT,run)
    TX.prepare(planned,ROOT/'Tools/Validation/Data/MonsterVariations.json')
    (run/'before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
    for definition in SPEC['skills']:
        sid=definition['id']
        if sid in by_skill: assert by_skill[sid]['Name']=='MonsterVariation_'+str(sid),sid
        row=copy.deepcopy(by_skill[definition['base']])
        anim=montage(definition)
        row.update(Name='MonsterVariation_'+str(sid),SkillID=sid,Desc=definition['name'],
                   MontagePath=ref(anim.get_path_name()),ElitePresentationMontage=anim.get_path_name(),
                   bSyncMontageToPattern=True,WindupMontageFraction=0.,ImpactMontageFraction=.45,MinimumBossPhase=1)
        row.update(definition['patch'])
        row['AimTrackingSeconds']=row['TelegraphDuration']*.3
        by_skill[sid]=row
    golem_blend=golem_anim()
    golem_death=montage(dict(id=15350,sequence='/Game/ExternalAssets/Characters/Fantasy_Pack/Animations/2Without_Weapon/Anim_Dead_1_Without_Weapon'))
    golem_hit=montage(dict(id=15351,sequence='/Game/ExternalAssets/Characters/Fantasy_Pack/Animations/2Without_Weapon/Anim_Get_Hit_1_Without_Weapon'))
    startup=duplicate(BASE+'Skeleton/DataAsset/DA_Enemy_Skeleton','DA_GolemStartUp')
    source_startup=unreal.load_asset(BASE+'Skeleton/DataAsset/DA_Enemy_Skeleton')
    reactive=[]
    for ability in source_startup.get_editor_property('ReactiveAbilities'):
        if isinstance(unreal.get_default_object(ability),unreal.PGAbilityHitReact):
            bp=duplicate(ability.get_path_name().split('.')[0],'GA_GolemHit')
            unreal.get_default_object(bp.generated_class()).set_editor_property('montage_paths',[unreal.SoftObjectPath(golem_hit.get_path_name())])
            unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
            reactive.append(bp.generated_class())
        else: reactive.append(ability)
    startup.set_editor_property('ReactiveAbilities',reactive)
    save(startup)
    definitions=list(SPEC['creatures'])
    for grade in SPEC['p09_grades']:
        for sex,eid in zip(('Female','Male'),grade['ids']):
            definitions.append(dict(id=eid,key='P09',name=grade['title'],source=15101,base=grade['base'],skills=grade['skills'],stats=grade['stats'],grade=grade,sex=sex))
    for d in definitions:
        eid=d['id']
        if eid in by_enemy and eid not in range(15201,15205):
            assert by_enemy[eid]['ActorClass'].startswith(DEST+'/BP_'),eid
        source=by_enemy[d['source']]['ActorClass'].split('.')[0]
        # Own the complete CDO; parent source assets remain untouched.
        bp=duplicate(source,'BP_'+str(eid))
        cdo=unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('character_tid',eid)
        cdo.set_editor_property('ai_controller_class',unreal.PGRoleAIController)
        if d['key']=='P09':
            cdo.appearance_component.set_editor_property('default_appearance',appearance(d['grade'],d['sex'],eid))
            from ConfigureHumanoidLocomotion import configure_enemy
            configure_enemy(cdo, bool(d['grade']['shield']))
        else:
            cdo.mesh.set_editor_property('relative_scale3d',unreal.Vector(d['scale'],d['scale'],d['scale']))
            if d['key']=='Golem':
                cdo.set_editor_property('character_start_up_data',startup)
                cdo.mesh.set_editor_property('skeletal_mesh_asset',unreal.load_asset('/Game/ExternalAssets/Characters/Fantasy_Pack/Characters/Golem/Mesh/SK_Golem'))
                cdo.set_editor_property('creature_locomotion',golem_blend)
                cdo.mesh.set_editor_property('anim_class',unreal.PGCreatureAnimInstance.static_class())
            bounds=cdo.mesh.get_editor_property('skeletal_mesh_asset').get_bounds()
            half=max(45.,bounds.box_extent.z*d['scale'])
            radius=min(85.,max(28.,min(bounds.box_extent.x,bounds.box_extent.y)*d['scale']*.8))
            cdo.capsule_component.set_capsule_size(radius,max(radius,half))
            cdo.mesh.set_editor_property('relative_location',unreal.Vector(0,0,-half))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
        row=copy.deepcopy(by_enemy[d['base']])
        row.update(Name='MonsterVariation_'+str(eid),EnemyID=eid,EnemyName=d['name'],ActorClass=bp.generated_class().get_path_name(),SkillIdList=d['skills'],Presentation='None')
        if d['key']=='Ent': row.update(GuardReduction=.35,GuardHalfAngle=65)
        if d.get('grade',{}).get('grade')=='Elite': row.update(GuardReduction=.55,DropPoolId='Rogue.Crusher')
        by_enemy[eid]=row
        stat=copy.deepcopy(next(r for r in data['stats'] if r['CharacterID']==d['base']))
        stat.update(Name='MonsterVariation_'+str(eid),CharacterID=eid)
        stat['Stats'].update(d['stats'])
        data['stats']=[r for r in data['stats'] if r['CharacterID']!=eid]+[stat]
        death=copy.deepcopy(next(r for r in data['deaths'] if r['ObjectTID']==d['source']))
        death.update(Name='MonsterVariation_'+str(eid),ObjectTID=eid)
        if d['key']=='Golem': death.update(DeathMontagePath=[ref(golem_death.get_path_name())],DissolveVFXPath='None')
        data['deaths']=[r for r in data['deaths'] if r['ObjectTID']!=eid]+[death]
    data['enemies']=list(by_enemy.values())
    data['skills']=list(by_skill.values())
    data['stages']=compose(data['stages'])
    for row in data['stats']:
        row['Stats']={k:(v['Stats'] if isinstance(v,dict) else v) for k,v in row['Stats'].items()}
    for key,path in TABLES.items():
        table=unreal.load_asset(path)
        assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(data[key],ensure_ascii=False)),key
        save(table)
    TX.data['status']='APPLIED_UNVERIFIED';TX.flush()
    validate()
    unreal.log('PGMonsterVariations APPLY PASS')

def validate():
    validate_grips()
    data={key:rows(path) for key,path in TABLES.items()}
    enemies={r['EnemyID']:r for r in data['enemies']}
    skills={r['SkillID']:r for r in data['skills']}
    stats={r['CharacterID']:{k:(v['Stats'] if isinstance(v,dict) else v) for k,v in r['Stats'].items()} for r in data['stats']}
    validate_roster(data['stages'])
    for definition in SPEC['skills']:
        row=skills[definition['id']]
        assert row['Desc']==definition['name']
        for key,value in definition['patch'].items():
            assert abs(row[key]-value)<1.e-4 if isinstance(value,(float,int)) else row[key]==value,(definition['id'],key)
        assert row['bSyncMontageToPattern'] and 0<=row['WindupMontageFraction']<row['ImpactMontageFraction']<1
    baseline=Path(os.environ.get('PG_MONSTER_RUN',str(ROOT/'Saved/MonsterVariations')))/'before.json'
    if baseline.exists():
        before=json.loads(baseline.read_text(encoding='utf-8'))
        for key,id_key,changed in [('enemies','EnemyID',IDS),('stats','CharacterID',IDS),('deaths','ObjectTID',IDS),('skills','SkillID',{d['id'] for d in SPEC['skills']})]:
            assert [r for r in before[key] if r[id_key] not in changed]==[r for r in data[key] if r[id_key] not in changed],key+' unrelated rows changed'
        assert compose(before['stages'])==data['stages'],'Non-roster stage fields changed'
    for eid in IDS:
        row=enemies[eid]
        cls=unreal.load_class(None,row['ActorClass'])
        cdo=unreal.get_default_object(cls)
        assert cdo.get_editor_property('character_tid')==eid
        assert cdo.get_editor_property('ai_controller_class')==unreal.PGRoleAIController.static_class()
        mesh=cdo.mesh.get_editor_property('skeletal_mesh_asset')
        assert mesh and cdo.mesh.get_editor_property('anim_class')
        for sid in row['SkillIdList']:
            anim=unreal.load_asset(skills[sid]['ElitePresentationMontage'])
            assert anim and anim.get_editor_property('skeleton')==mesh.get_editor_property('skeleton'),(eid,sid)
            assert not unreal.AnimationLibrary.get_animation_notify_events(anim)
            for track in anim.get_editor_property('slot_anim_tracks'):
                for segment in track.get_editor_property('anim_track').get_editor_property('anim_segments'):
                    assert not unreal.AnimationLibrary.get_animation_notify_events(segment.get_editor_property('anim_reference'))
        assert stats[eid]['Health']>0 and row['DropPoolId']!='None'
        death=next(r for r in data['deaths'] if r['ObjectTID']==eid)
        for path in death['DeathMontagePath']:
            anim=unreal.load_asset(path['AssetPath']['PackageName'])
            assert anim and anim.get_editor_property('skeleton')==mesh.get_editor_property('skeleton'),(eid,'death skeleton')
    for grade in SPEC['p09_grades']:
        for eid in grade['ids']:
            assert all(stats[eid][k]==v for k,v in grade['stats'].items())
            cdo=unreal.get_default_object(unreal.load_class(None,enemies[eid]['ActorClass']))
            appearance=cdo.appearance_component.get_editor_property('default_appearance')
            assert appearance and len(appearance.get_editor_property('attachments'))==(2 if grade['shield'] else 1)
            assert sum('_Armor_'+grade['armor']+'_' in p.mesh.get_path_name() for p in appearance.get_editor_property('parts'))==5
            assert enemies[eid]['SkillIdList']==grade['skills'] and enemies[eid]['EnemyName']==grade['title']
    for creature in SPEC['creatures']:
        eid=creature['id']
        assert enemies[eid]['SkillIdList']==creature['skills']
        assert all(stats[eid][k]==v for k,v in creature['stats'].items())
        if creature['key']!='Golem':
            # Matching skeletons alone do not establish model-local provenance.
            prefix='/Game/ExternalAssets/Characters/Enemies/'+creature['key']+'/Animations/'
            cdo=unreal.get_default_object(unreal.load_class(None,enemies[eid]['ActorClass']))
            defaults=unreal.get_default_object(cdo.mesh.get_editor_property('anim_class'))
            for field in ('DefaultBlendSpace','StrafingBlendSpace'):
                movement=defaults.get_editor_property(field)
                assert movement,(eid,field)
                samples=list(movement.get_editor_property('sample_data'))
                assert samples,(eid,field,'empty samples')
                for sample in samples:
                    clip=sample.get_editor_property('animation')
                    assert clip and clip.get_path_name().startswith(prefix),(eid,field,clip)
                    assert unreal.PGHumanoidLocomotionTools.get_blend_sample_count(movement,sample.get_editor_property('sample_value'))>0,(eid,field,'empty interpolation')
            for definition in SPEC['skills']:
                if definition['id'] not in creature['skills']:continue
                source=unreal.load_asset(BASE+definition['montage'])
                for track in source.get_editor_property('slot_anim_tracks'):
                    for segment in track.get_editor_property('anim_track').get_editor_property('anim_segments'):
                        assert segment.get_editor_property('anim_reference').get_path_name().startswith(prefix),(eid,definition['id'],'non-local source')
    golem=unreal.get_default_object(unreal.load_class(None,enemies[15305]['ActorClass']))
    anim=unreal.get_default_object(golem.mesh.get_editor_property('anim_class'))
    assert isinstance(anim,unreal.PGCreatureAnimInstance)
    assert golem.mesh.get_editor_property('anim_class')==unreal.PGCreatureAnimInstance.static_class()
    blend=golem.get_editor_property('creature_locomotion')
    assert blend and len(blend.get_editor_property('sample_data'))==3
    for speed in (0.,75.,150.,225.,255.,300.):
        assert unreal.PGHumanoidLocomotionTools.get_blend_sample_count(blend,unreal.Vector(speed,0,0))>0,('Golem empty locomotion',speed)
    assert blend.get_editor_property('skeleton')==golem.mesh.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
    startup=golem.get_editor_property('character_start_up_data')
    assert startup and startup.get_path_name().startswith(DEST+'/DA_GolemStartUp')
    for ability in startup.get_editor_property('ReactiveAbilities'):
        cdo=unreal.get_default_object(ability)
        if isinstance(cdo,unreal.PGAbilityHitReact):
            for path in cdo.get_editor_property('montage_paths'):
                montage_asset=unreal.load_asset(path.export_text().strip('"'))
                assert montage_asset.get_editor_property('skeleton')==blend.get_editor_property('skeleton')
    from ValidateLootPools import validate_loot_pools
    validate_loot_pools(unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression'),enemies)
    unreal.log('PGMonsterVariations VALIDATION PASS')

if __name__=='__main__':
    command=unreal.SystemLibrary.get_command_line()
    if '-PGMonsterGripsValidate' in command:validate_grips()
    elif '-PGMonsterGripsOnly' in command:apply_grips()
    elif '-PGMonsterValidate' in command:validate()
    else:apply()
