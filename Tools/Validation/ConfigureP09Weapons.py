"""P09 weapon derivatives. All writes are declared and backed up before authoring."""
import copy
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from P09WeaponRoster import SPEC, SPEC_PATH, DEST, IDS, compose, validate_roster
from ConfigureMonsterVariations import TABLES, rows, ref
from PlayableCharacterTransaction import Transaction, write_json
from ConfigureAttackMotion import configure_montage_blend
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
OUT = Path(os.environ.get('PG_P09_WEAPON_RUN', str(ROOT/'Saved/P09Weapons')))
TX = None


def save(asset):
    TX.mark_written(asset.get_path_name().split('.')[0])
    assert EAL.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def duplicate(source, name):
    path=DEST+'/'+name
    obj=unreal.load_asset(path) if EAL.does_asset_exist(path) else EAL.duplicate_asset(source,path)
    assert obj, (source,path)
    return obj


def own(name, cls, factory):
    path=DEST+'/'+name
    obj=unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(name,DEST,cls,factory)
    assert obj,path
    return obj


def motion_entries():
    meshes={}; entries={}
    def add(key,path,mesh):
        pack=next((p for p,m in meshes.items() if m==mesh),None)
        if pack is None: pack='Source'+str(len(meshes)); meshes[pack]=mesh
        entries[key]=dict(source=path,pack=pack)
    for family,data in SPEC['families'].items():
        for key,path in data.get('motions',{}).items():add(family+'_'+key,path,data['source_mesh'])
    for skill in SPEC['skills']:add(skill.get('motion_key',str(skill['id'])),skill['source'],skill['source_mesh'])
    return meshes,entries


def inspect():
    data={key:rows(path) for key,path in TABLES.items()}
    meshes,entries=motion_entries()
    inventory=ROOT/'Docs/todo/assets/MotionExpansion/art-animation-inventory.csv'
    catalog=inventory.read_text(encoding='utf-8-sig')
    records=[]
    for key,e in entries.items():
        assert e['source'] in catalog, ('Not in approved motion inventory',e['source'])
        clip=unreal.load_asset(e['source']);mesh=unreal.load_asset(meshes[e['pack']])
        assert clip and mesh
        assert clip.get_editor_property('skeleton')==mesh.get_editor_property('skeleton'),key
        records.append(dict(key=key,source=e['source'],seconds=clip.get_play_length(),skeleton=str(clip.get_editor_property('skeleton'))))
    bounds=[]
    for i in range(1,5):
        asset=unreal.load_asset('/Game/Art/P09Modular/Meshes/SM_P09_Weapon_Staff_'+str(i).zfill(3))
        b=asset.get_bounds()
        bounds.append(dict(asset=asset.get_path_name(),origin=str(b.origin),extent=str(b.box_extent)))
    compose(data['stages'])
    write_json(OUT/'inspection.json',dict(motions=records,staff_bounds=bounds,enemies=data['enemies'],skills=[r for r in data['skills'] if r['SkillID'] in (15101,15102,15105,15111,15114)],stages=data['stages']))


def weapon_name(family,tier):return 'SM_PG_'+family+'_'+str(tier)


def import_weapons():
    materials={}
    for key,color,metal,rough in [('Steel',(.25,.34,.43),.8,.3),('Edge',(.63,.72,.78),.85,.22),('Grip',(.09,.035,.018),.05,.72),('Gold',(.52,.30,.075),.75,.32)]:
        asset=own('M_'+key,unreal.Material,unreal.MaterialFactoryNew())
        unreal.MaterialEditingLibrary.delete_all_material_expressions(asset)
        node=unreal.MaterialEditingLibrary.create_material_expression(asset,unreal.MaterialExpressionConstant3Vector)
        node.set_editor_property('constant',unreal.LinearColor(*color,1))
        unreal.MaterialEditingLibrary.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
        for value,prop in [(metal,unreal.MaterialProperty.MP_METALLIC),(rough,unreal.MaterialProperty.MP_ROUGHNESS)]:
            n=unreal.MaterialEditingLibrary.create_material_expression(asset,unreal.MaterialExpressionConstant)
            n.set_editor_property('r',value);unreal.MaterialEditingLibrary.connect_material_property(n,'',prop)
        unreal.MaterialEditingLibrary.recompile_material(asset);save(asset);materials[key]=asset
    for family in ('Spear','Greatsword'):
        for tier in range(3):
            name=weapon_name(family,tier)
            task=unreal.AssetImportTask();task.filename=str(ROOT/'Tools/Art/P09Weapons'/(name+'.fbx'))
            task.destination_path=DEST;task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=False
            options=unreal.FbxImportUI();options.import_mesh=True;options.import_materials=False;options.import_textures=False
            options.import_as_skeletal=False;options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
            options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=False
            task.options=options;TOOLS.import_asset_tasks([task])
            asset=unreal.load_asset(DEST+'/'+name);assert asset
            for i,slot in enumerate(asset.get_editor_property('static_materials')):
                key=str(slot.get_editor_property('material_slot_name')).split('.')[0]
                assert key in materials,(name,key)
                asset.set_material(i,materials[key])
            save(asset)


def make_blend(family,clips):
    factory=unreal.BlendSpaceFactoryNew();factory.set_editor_property('target_skeleton',clips[family+'_Idle'].get_editor_property('skeleton'))
    asset=own('BS_'+family,unreal.BlendSpace,factory)
    speed=unreal.BlendParameter()
    max_speed=350. if family=='Spear' else 305.
    for k,v in dict(display_name='Speed',min=0.,max=max_speed).items():speed.set_editor_property(k,v)
    direction=unreal.BlendParameter()
    for k,v in dict(display_name='Direction',min=-180.,max=180.,grid_num=8,wrap_input=True).items():direction.set_editor_property(k,v)
    asset.set_editor_property('blend_parameters',[direction,speed,speed])
    samples=[]
    for angle,suffix in [(-180,'B'),(-135,'BL'),(-90,'L'),(-45,'FL'),(0,'F'),(45,'FR'),(90,'R'),(135,'BR'),(180,'B')]:
        for velocity,key in [(0,'Idle'),(160,'Walk_'+suffix),(max_speed,'Run_'+suffix)]:
            sample=unreal.BlendSample();sample.set_editor_property('animation',clips[family+'_'+key]);sample.set_editor_property('sample_value',unreal.Vector(angle,velocity,0))
            samples.append(sample)
    asset.set_editor_property('sample_data',samples);asset.set_editor_property('target_weight_interpolation_speed_per_sec',8.)
    unreal.PGHumanoidLocomotionTools.rebuild_blend_space(asset);save(asset)
    return asset


def attachments(monster):
    tier=['Normal','Veteran','Elite'].index(monster['grade'])
    family=monster['family'];profile=SPEC['grips'][monster['sex']]
    asset='/Game/Art/P09Modular/Meshes/SM_P09_Weapon_Staff_'+str(tier+1).zfill(3) if family=='Staff' else DEST+'/'+weapon_name(family,tier)
    gear=unreal.PGAppearanceAttachment();gear.mesh=unreal.load_asset(asset);assert gear.mesh
    gear.attach_bone='hand_r'
    t=unreal.Transform(location=unreal.Vector(*profile['translation']),scale=unreal.Vector(*profile['scale']))
    pitch,yaw,roll=profile['rotation']
    t.rotation=unreal.Rotator(pitch=pitch,yaw=yaw,roll=roll).quaternion()
    gear.relative_transform=t
    return [gear]


def grips():
    global TX
    TX=Transaction(ROOT,OUT);TX.prepare([DEST+'/DA_'+str(eid) for eid in IDS],SPEC_PATH)
    for d in SPEC['monsters']:
        appearance=unreal.load_asset(DEST+'/DA_'+str(d['id']));assert appearance
        appearance.set_editor_property('attachments',attachments(d));save(appearance)
    TX.data['status']='APPLIED_UNVERIFIED';TX.flush();validate()


def apply():
    global TX
    before={key:rows(path) for key,path in TABLES.items()};data=copy.deepcopy(before)
    enemies={r['EnemyID']:r for r in data['enemies']};skills={r['SkillID']:r for r in data['skills']}
    for eid in IDS:assert eid not in enemies or enemies[eid]['Name']=='P09Weapon_'+str(eid),eid
    for d in SPEC['skills']:assert d['id'] not in skills or skills[d['id']]['Name']=='P09Weapon_'+str(d['id']),d['id']
    compose(data['stages'])
    meshes,entries=motion_entries();motion_dest=DEST+'/Motion'
    packages=list(TABLES.values())+[DEST+'/'+prefix+str(eid) for eid in IDS for prefix in ('BP_','DA_')]
    packages += [DEST+'/M_'+k for k in ('Steel','Edge','Grip','Gold')]
    packages += [DEST+'/'+weapon_name(f,t) for f in ('Spear','Greatsword') for t in range(3)]
    packages += [DEST+'/BS_'+f for f in ('Spear','Greatsword')]
    packages += [motion_dest+'/IK_Target']+[motion_dest+'/'+prefix+p for p in meshes for prefix in ('IK_','RTG_')]
    packages += [motion_dest+'/AS_'+key for key in entries]+[motion_dest+'/PGSwordSaint_'+e['source'].rsplit('/',1)[1] for e in entries.values()]
    packages += [motion_dest+'/AM_'+str(d['id']) for d in SPEC['skills']]
    TX=Transaction(ROOT,OUT);TX.prepare(packages,SPEC_PATH);write_json(OUT/'before.json',before)
    import_weapons()
    import ConfigureHumanoidBoss as motion
    motion.DEST=motion_dest;motion.RUN=OUT;motion.TX=TX;motion.SOURCE_MESHES=meshes;motion.ENTRIES=entries
    motion.SPEC=dict(contact_fractions={str(d['id']):d['contact'] for d in SPEC['skills']})
    original_rig=motion.rig
    def source_rig(name,mesh):
        if mesh.get_path_name().split('.')[0]!='/Game/Art/AnimationTests/RPGAnimations/Setup/RPG/SK_PGRPG':
            return original_rig(name,mesh)
        # RPG uses Hips/Spine/Left_Hand, not the UE mannequin names. Its authored
        # source rig already maps these bones to the same semantic chain names.
        path=motion_dest+'/'+name
        rig=unreal.load_asset(path) if EAL.does_asset_exist(path) else EAL.duplicate_asset('/Game/Art/AnimationTests/RPGAnimations/Setup/RPG/IK_PGRPG',path)
        assert rig;save(rig)
        return rig,[]
    motion.rig=source_rig
    target=unreal.load_asset('/Game/ExternalAssets/Characters/Enemies/SkeletonEnemy/Mesh/SK_Skeleton')
    clips=motion.retarget(target)
    blends={family:make_blend(family,clips) for family in ('Spear','Greatsword')}
    for d in SPEC['skills']:
        sid=d['id'];montage=motion.montage(str(sid),clips[d.get('motion_key',str(sid))]);configure_montage_blend(montage,'DefaultSlot');save(montage)
        row=copy.deepcopy(skills[d['base']]);row.update(d['patch'])
        row.update(Name='P09Weapon_'+str(sid),SkillID=sid,Desc=d['name'],MontagePath=ref(montage.get_path_name()),ElitePresentationMontage=montage.get_path_name(),
                   EnemyProfile='None',PlayerProfile='None',bSyncMontageToPattern=True,WindupMontageFraction=0.,ImpactMontageFraction=d['contact'],MinimumBossPhase=1,ChainSkillIdList=[],
                   AimTrackingSeconds=row['TelegraphDuration']*.25,RecoveryDamageBonus=.2)
        row['SelectionTuning'].update(PreferredRange=700. if d['family']=='Staff' and sid!=15759 else 240.,RangeFalloff=500.,RecentUseSeconds=6.)
        if 'tempo' in d:
            row.update(MotionTempo=d['tempo'], AimTurnRate=d['aim_turn_rate'])
        skills[sid]=row
    for d in SPEC['monsters']:
        eid=d['id'];tier=['Normal','Veteran','Elite'].index(d['grade']);source_id=15201 if d['sex']=='Female' else 15202
        bp=duplicate(enemies[source_id]['ActorClass'].split('.')[0],'BP_'+str(eid));cdo=unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('character_tid',eid);cdo.set_editor_property('ai_controller_class',unreal.PGRoleAIController)
        cdo.set_editor_property('creature_locomotion',blends['Spear' if d['family']=='Staff' else d['family']])
        cdo.mesh.set_editor_property('anim_class',unreal.PGCreatureAnimInstance.static_class())
        source=unreal.load_asset('/Game/DataCenter/Characters/DA_P09_'+d['sex'])
        appearance=duplicate(source.get_path_name(),'DA_'+str(eid))
        for field in ('mesh','source_mesh','retargeter','mesh_transform','head_bone','head_forward_axis','head_right_axis','equipment_bones','reconstruct_scaled_translations'):
            appearance.set_editor_property(field,source.get_editor_property(field))
        appearance.set_editor_property('id','P09Weapon_'+str(eid));appearance.set_editor_property('display_name',d['name'])
        parts=[];prefix='Fem' if d['sex']=='Female' else 'Male'
        for label in ('Head','Chest','Arm','Waist','Leg'):
            part=unreal.PGAppearancePart();part.mesh=unreal.load_asset('/Game/Art/P09Modular/Meshes/SK_P09_'+prefix+'_Armor_'+d['armor']+'_'+label);assert part.mesh
            part.relative_transform=unreal.Transform();parts.append(part)
        parts.extend(p for p in source.get_editor_property('parts') if str(p.attach_bone)!='None')
        appearance.set_editor_property('parts',parts);appearance.set_editor_property('attachments',attachments(d));save(appearance)
        cdo.appearance_component.set_editor_property('default_appearance',appearance)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
        row=copy.deepcopy(enemies[d['base']]);row.update(Name='P09Weapon_'+str(eid),EnemyID=eid,EnemyName=d['name'],ActorClass=bp.generated_class().get_path_name(),
            SkillIdList=d['skills'],DropPoolId=d['drop_pool'],Presentation='None',Mobility='Ground',GuardReduction=0.,SelectionReconsiderSeconds=3.,
            PreferredDistance=650. if d['family']=='Staff' else 210. if d['family']=='Spear' else 180.)
        row['Positioning'].update(bEnabled=tier>0,ReconsiderSeconds=1.5,Separation=150.,MaxMoveDistance=220.,MinimumImprovement=50.)
        enemies[eid]=row
        stat=copy.deepcopy(next(r for r in data['stats'] if r['CharacterID']==d['base']))
        stat.update(Name='P09Weapon_'+str(eid),CharacterID=eid)
        stat['Stats']={k:(v['Stats'] if isinstance(v,dict) else v) for k,v in stat['Stats'].items()};stat['Stats'].update(d['stats'])
        data['stats']=[r for r in data['stats'] if r['CharacterID']!=eid]+[stat]
        death=copy.deepcopy(next(r for r in data['deaths'] if r['ObjectTID']==source_id));death.update(Name='P09Weapon_'+str(eid),ObjectTID=eid)
        data['deaths']=[r for r in data['deaths'] if r['ObjectTID']!=eid]+[death]
    data['enemies']=list(enemies.values());data['skills']=list(skills.values());data['stages']=compose(data['stages'])
    # The stat row's custom JSON importer expects scalars; export wraps them.
    for stat in data['stats']:
        stat['Stats']={k:(v['Stats'] if isinstance(v,dict) else v) for k,v in stat['Stats'].items()}
    for key,path in TABLES.items():
        table=unreal.load_asset(path);assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(data[key],ensure_ascii=False)),key
        save(table)
    TX.data['status']='APPLIED_UNVERIFIED';TX.flush()
    validate()


def validate():
    data={key:rows(path) for key,path in TABLES.items()};enemies={r['EnemyID']:r for r in data['enemies']};skills={r['SkillID']:r for r in data['skills']}
    stats={r['CharacterID']:{k:(v['Stats'] if isinstance(v,dict) else v) for k,v in r['Stats'].items()} for r in data['stats']}
    validate_roster(data['stages']);records=[]
    for d in SPEC['monsters']:
        eid=d['id'];row=enemies[eid];assert row['SkillIdList']==d['skills'] and row['DropPoolId']==d['drop_pool'],eid
        cdo=unreal.get_default_object(unreal.load_class(None,row['ActorClass']));assert cdo.get_editor_property('character_tid')==eid
        assert cdo.get_editor_property('ai_controller_class')==unreal.PGRoleAIController.static_class()
        appearance=cdo.appearance_component.get_editor_property('default_appearance');assert appearance
        assert appearance.get_editor_property('attachments')[0].export_text()==attachments(d)[0].export_text(),eid
        assert sum('_Armor_'+d['armor']+'_' in p.mesh.get_path_name() for p in appearance.get_editor_property('parts'))==5,eid
        assert all(abs(stats[eid][k]-v)<.001 for k,v in d['stats'].items()),eid
        skeleton=cdo.mesh.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
        blend=cdo.get_editor_property('creature_locomotion');assert blend.get_editor_property('skeleton')==skeleton
        assert len(blend.get_editor_property('sample_data'))==27
        for sample in blend.get_editor_property('sample_data'):
            assert unreal.PGHumanoidLocomotionTools.get_blend_sample_count(blend,sample.get_editor_property('sample_value'))>0
        for sid in d['skills']:
            montage=unreal.load_asset(skills[sid]['ElitePresentationMontage']);assert montage.get_editor_property('skeleton')==skeleton
            assert str(montage.get_editor_property('slot_anim_tracks')[0].slot_name)=='DefaultSlot'
            assert not unreal.AnimationLibrary.get_animation_notify_events(montage)
        death=next(r for r in data['deaths'] if r['ObjectTID']==eid)
        for path in death['DeathMontagePath']:assert unreal.load_asset(path['AssetPath']['PackageName']).get_editor_property('skeleton')==skeleton
        records.append(dict(enemy=eid,name=d['name'],grade=d['grade'],family=d['family'],sex=d['sex'],skills=d['skills']))
    for d in SPEC['skills']:
        row=skills[d['id']]
        for key,value in d['patch'].items():assert abs(row[key]-value)<.0001 if isinstance(value,(int,float)) else row[key]==value,(d['id'],key)
        assert abs(row['ImpactMontageFraction']-d['contact'])<.0001
        if 'tempo' in d:
            assert abs(row['AimTurnRate']-d['aim_turn_rate'])<.0001
            for key,value in d['tempo'].items():assert abs(row['MotionTempo'][key]-value)<.0001,(d['id'],key)
        clip=unreal.load_asset(DEST+'/Motion/AS_'+d.get('motion_key',str(d['id'])))
        assert not clip.get_editor_property('enable_root_motion') and not unreal.AnimationLibrary.get_animation_notify_events(clip)
        poses=[clip.get_anim_pose_at_time(clip.get_play_length()*f,unreal.AnimPoseEvaluationOptions()) for f in (0,.25,.5,.75)]
        points=[unreal.AnimPoseExtensions.get_bone_pose(p,'hand_r',unreal.AnimPoseSpaces.WORLD).translation for p in poses]
        assert max((p-points[0]).length() for p in points)>2.,(d['id'],'static retarget')
        assert all(math.isfinite(v) for p in points for v in (p.x,p.y,p.z))
    before_path=OUT/'before.json'
    if before_path.exists():
        before=json.loads(before_path.read_text(encoding='utf-8'))
        for key,id_key,changed in [('enemies','EnemyID',IDS),('stats','CharacterID',IDS),('deaths','ObjectTID',IDS),('skills','SkillID',{d['id'] for d in SPEC['skills']})]:
            assert [r for r in before[key] if r[id_key] not in changed]==[r for r in data[key] if r[id_key] not in changed],key+' unrelated data changed'
        assert compose(before['stages'])==data['stages'],'Non-spawn stage data changed'
    from ValidateLootPools import validate_loot_pools
    validate_loot_pools(unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression'),enemies)
    write_json(OUT/'validation.json',dict(status='PASS',monsters=records,skill_count=len(SPEC['skills'])))
    _,entries=motion_entries()
    with (OUT/'motion-use-manifest.csv').open('w',encoding='utf-8-sig',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=['key','source','sequence','montage','enemy_ids','validation']);writer.writeheader()
        for key,e in entries.items():
            if key.isdigit():continue
            family=key.split('_')[0]
            consumers=[str(m['id']) for m in SPEC['monsters'] if m['family']==family or (family=='Spear' and m['family']=='Staff')]
            writer.writerow(dict(key=key,source=e['source'],sequence=DEST+'/Motion/AS_'+key,montage='',enemy_ids=';'.join(consumers),validation='saved_reload'))
        for d in SPEC['skills']:
            consumers=[str(m['id']) for m in SPEC['monsters'] if d['id'] in m['skills']]
            writer.writerow(dict(key=d['id'],source=d['source'],sequence=DEST+'/Motion/AS_'+d.get('motion_key',str(d['id'])),montage=DEST+'/Motion/AM_'+str(d['id']),enemy_ids=';'.join(consumers),validation='saved_reload'))


if __name__=='__main__':
    step=os.environ.get('PG_P09_WEAPON_STEP','validate');OUT.mkdir(parents=True,exist_ok=True)
    {'inspect':inspect,'apply':apply,'validate':validate,'grips':grips}[step]()
    unreal.log('PGP09Weapons '+step.upper()+' PASS')
