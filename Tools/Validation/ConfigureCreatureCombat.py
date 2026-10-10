"""Register creature combat assets without changing the imported gallery assets."""
import copy, json, os, sys, traceback
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from PlayableCharacterTransaction import Transaction
from ConfigureMonsterVariations import TABLES, rows, ref
SPEC=json.loads((ROOT/'Tools/Validation/Data/CreatureCombat.json').read_text(encoding='utf-8'))
DEST='/Game/DataCenter/CreatureCombat'
ART='/Game/Art/CreatureModels'
OUT=Path(os.environ.get('PG_CREATURE_RUN',str(ROOT/'Saved/CreatureCombat')))
EAL=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
TX=None

def save(asset):
    TX.mark_written(asset.get_path_name().split('.')[0])
    assert EAL.save_loaded_asset(asset,only_if_is_dirty=False)

def own(name,cls,factory):
    path=DEST+'/'+name
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(name,DEST,cls,factory)

def duplicate(source,name):
    path=DEST+'/'+name
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else EAL.duplicate_asset(source,path)

def motion(model,clip,montage=True):
    return ART+'/'+model+('/Montages/AM_' if montage else '/Animations/AS_')+'PG_'+model+'_'+clip

def apply():
    global TX
    before={key:rows(path) for key,path in TABLES.items()}
    data=copy.deepcopy(before)
    enemies={r['EnemyID']:r for r in data['enemies']}
    skills={r['SkillID']:r for r in data['skills']}
    planned=list(TABLES.values())
    for d in SPEC['monsters']:
        planned += [DEST+'/'+prefix+str(d['id']) for prefix in ('BP_','BS_','DA_','GA_Hit_')]
    TX=Transaction(ROOT,OUT);TX.prepare(planned,ROOT/'Tools/Validation/Data/CreatureCombat.json')
    (OUT/'before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
    for d in SPEC['skills']:
        sid=d['id']
        assert sid not in skills or skills[sid]['Name']=='CreatureCombat_'+str(sid)
        am=unreal.load_asset(motion(d['model'],d['clip']));assert am
        row=copy.deepcopy(skills[d['base']])
        row.update(Name='CreatureCombat_'+str(sid),SkillID=sid,Desc=d['name'],MontagePath=ref(am.get_path_name()),ElitePresentationMontage=am.get_path_name(),
                   EnemyProfile='None',bSyncMontageToPattern=True,WindupMontageFraction=0.,ImpactMontageFraction=.45,MinimumBossPhase=1)
        row.update(d['patch']);skills[sid]=row
    source_startup=unreal.load_asset('/Game/DataCenter/MonsterVariations/DA_GolemStartUp');assert source_startup
    for d in SPEC['monsters']:
        eid=d['id'];model=d['model'];suffix=str(eid)
        assert eid not in enemies or enemies[eid]['Name']=='CreatureCombat_'+suffix
        mesh=unreal.load_asset(ART+'/'+model+'/SK_PG_'+model);assert mesh
        bf=unreal.BlendSpaceFactory1D();bf.set_editor_property('target_skeleton',mesh.get_editor_property('skeleton'))
        blend=own('BS_'+suffix,unreal.BlendSpace1D,bf)
        axis=unreal.BlendParameter();axis.set_editor_property('display_name','Speed');axis.set_editor_property('max',max(1.,d['stats']['MovementSpeed']))
        blend.set_editor_property('blend_parameters',[axis,axis,axis]);samples=[]
        for speed,clip in [(0,d['idle']),(axis.get_editor_property('max'),d['move'])]:
            sample=unreal.BlendSample();sample.set_editor_property('animation',unreal.load_asset(motion(model,clip,False)))
            sample.set_editor_property('sample_value',unreal.Vector(speed,0,0));samples.append(sample)
        blend.set_editor_property('sample_data',samples);unreal.PGHumanoidLocomotionTools.rebuild_blend_space(blend);save(blend)
        startup=duplicate(source_startup.get_path_name(),'DA_'+suffix)
        startup.set_editor_property('ActivateOnGivenAbilities',[])
        reactive=[]
        for ability in source_startup.get_editor_property('ReactiveAbilities'):
            if isinstance(unreal.get_default_object(ability),unreal.PGAbilityHitReact):
                hit=duplicate(ability.get_path_name().split('.')[0],'GA_Hit_'+suffix)
                unreal.get_default_object(hit.generated_class()).set_editor_property('montage_paths',[unreal.SoftObjectPath(motion(model,'Hit'))])
                unreal.BlueprintEditorLibrary.compile_blueprint(hit);save(hit);reactive.append(hit.generated_class())
            else:reactive.append(ability)
        startup.set_editor_property('ReactiveAbilities',reactive);save(startup)
        factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.PGCharacterEnemy)
        bp=own('BP_'+suffix,unreal.Blueprint,factory);cdo=unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('character_tid',eid);cdo.set_editor_property('ai_controller_class',unreal.PGRoleAIController)
        cdo.set_editor_property('character_start_up_data',startup);cdo.set_editor_property('creature_locomotion',blend)
        source_cdo=unreal.get_default_object(unreal.load_class(None,enemies[d['base']]['ActorClass']))
        cdo.set_editor_property('feedback_data',source_cdo.get_editor_property('feedback_data'))
        nameplate=cdo.get_editor_property('enemy_nameplate_widget_component')
        source_nameplate=source_cdo.get_editor_property('enemy_nameplate_widget_component')
        for field in ('widget_class','space','draw_size','draw_at_desired_size'):
            nameplate.set_editor_property(field,source_nameplate.get_editor_property(field))
        cdo.mesh.set_skeletal_mesh_asset(mesh);cdo.mesh.set_editor_property('anim_class',unreal.PGCreatureAnimInstance.static_class())
        cdo.mesh.set_editor_property('animation_mode',unreal.AnimationMode.ANIMATION_BLUEPRINT)
        bounds=mesh.get_bounds();scale=d['height']/max(1.,2*bounds.box_extent.z);half=d['height']*.5
        cdo.capsule_component.set_capsule_size(d['radius'],max(d['radius'],half))
        nameplate.set_editor_property('relative_location',unreal.Vector(0,0,half+30))
        cdo.mesh.set_editor_property('relative_scale3d',unreal.Vector(scale,scale,scale))
        cdo.mesh.set_editor_property('relative_location',unreal.Vector(0,0,-half-(bounds.origin.z-bounds.box_extent.z)*scale))
        cdo.mesh.set_editor_property('relative_rotation',unreal.Rotator())
        cdo.mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
        row=copy.deepcopy(enemies[d['base']]);row.update(Name='CreatureCombat_'+suffix,EnemyID=eid,EnemyName=d['name'],ActorClass=bp.generated_class().get_path_name(),
            SkillIdList=d['skills'],Mobility=d['mobility'],FlightHeight=60.,Presentation='None',CombatBehaviorTree='None',PreferredDistance=0.)
        row['Positioning']['bEnabled']=False
        enemies[eid]=row
        stat=copy.deepcopy(next(r for r in data['stats'] if r['CharacterID']==d['base']))
        stat.update(Name='CreatureCombat_'+suffix,CharacterID=eid)
        stat['Stats']={k:(v['Stats'] if isinstance(v,dict) else v) for k,v in stat['Stats'].items()}
        stat['Stats'].update(d['stats']);data['stats']=[r for r in data['stats'] if r['CharacterID']!=eid]+[stat]
        death=copy.deepcopy(next(r for r in data['deaths'] if r['ObjectTID']==15101))
        death.update(Name='CreatureCombat_'+suffix,ObjectTID=eid,DeathMontagePath=[ref(motion(model,d['death']))],DissolveVFXPath='None')
        data['deaths']=[r for r in data['deaths'] if r['ObjectTID']!=eid]+[death]
    data['enemies']=list(enemies.values());data['skills']=list(skills.values())
    for patch in SPEC['wave_replacements']:
        stage=next(r for r in data['stages'] if r['Id']==patch['stage'])
        infos=stage['Waves'][patch['wave']-1]['MonsterSpawnInfos']
        if any(r['MonsterId']==patch['enemy'] for r in infos):continue
        source=next(r for r in infos if r['MonsterId']==patch['source']);new=copy.deepcopy(source)
        source['SpawnCount']-=1;new.update(MonsterId=patch['enemy'],SpawnCount=1)
        infos[:]=[r for r in infos if r['SpawnCount']>0]+[new]
    for row in data['stats']:row['Stats']={k:(v['Stats'] if isinstance(v,dict) else v) for k,v in row['Stats'].items()}
    for key,path in TABLES.items():
        table=unreal.load_asset(path)
        assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(data[key],ensure_ascii=False)),key
        save(table)
    TX.data['status']='APPLIED_UNVERIFIED';TX.flush()

def validate():
    data={key:rows(path) for key,path in TABLES.items()}
    enemies={r['EnemyID']:r for r in data['enemies']};skills={r['SkillID']:r for r in data['skills']}
    for d in SPEC['skills']:
        row=skills[d['id']]
        for key,value in d['patch'].items():
            assert abs(row[key]-value)<1.e-5 if isinstance(value,(int,float)) else row[key]==value,(d['id'],key)
    for d in SPEC['monsters']:
        row=enemies[d['id']];assert row['Mobility']==d['mobility'] and row['SkillIdList']==d['skills']
        cdo=unreal.get_default_object(unreal.load_class(None,row['ActorClass']))
        assert cdo.get_editor_property('character_tid')==d['id']
        assert cdo.get_editor_property('ai_controller_class')==unreal.PGRoleAIController.static_class()
        mesh=cdo.mesh.get_editor_property('skeletal_mesh_asset');assert mesh
        for sid in d['skills']:
            am=unreal.load_asset(skills[sid]['ElitePresentationMontage']);assert am.get_editor_property('skeleton')==mesh.get_editor_property('skeleton')
        blend=cdo.get_editor_property('creature_locomotion')
        assert unreal.PGHumanoidLocomotionTools.get_blend_sample_count(blend,unreal.Vector(0,0,0))>0
        assert row['DropPoolId']!='None'
        stat=next(r for r in data['stats'] if r['CharacterID']==d['id'])['Stats']
        assert all((stat[k]['Stats'] if isinstance(stat[k],dict) else stat[k])==v for k,v in d['stats'].items())
        death=next(r for r in data['deaths'] if r['ObjectTID']==d['id'])
        for p in death['DeathMontagePath']:
            assert unreal.load_asset(p['AssetPath']['PackageName']).get_editor_property('skeleton')==mesh.get_editor_property('skeleton')
    assert skills[15523]['SummonEnemyID']==15504 and skills[15523]['MaxLivingSummons']==4
    from MonsterVariationRoster import validate_roster
    validate_roster(data['stages'])
    if not (OUT/'before.json').exists():
        unreal.log('PGCreatureCombat VALIDATION PASS');return
    baseline=json.loads((OUT/'before.json').read_text(encoding='utf-8'))
    for key,id_key,changed in [('enemies','EnemyID',{d['id'] for d in SPEC['monsters']}),('skills','SkillID',{d['id'] for d in SPEC['skills']}),('stats','CharacterID',{d['id'] for d in SPEC['monsters']}),('deaths','ObjectTID',{d['id'] for d in SPEC['monsters']})]:
        assert [r for r in baseline[key] if r[id_key] not in changed]==[r for r in data[key] if r[id_key] not in changed],key
    for old,new in zip(baseline['stages'],data['stages']):
        assert len(old['Waves'])==len(new['Waves'])
        for a,b in zip(old['Waves'],new['Waves']):assert sum(r['SpawnCount'] for r in a['MonsterSpawnInfos'])==sum(r['SpawnCount'] for r in b['MonsterSpawnInfos'])
    unreal.log('PGCreatureCombat VALIDATION PASS')

if __name__=='__main__':
    try:
        if '-PGCreatureValidate' not in unreal.SystemLibrary.get_command_line():apply()
        validate()
        (OUT/('reload.json' if '-PGCreatureValidate' in unreal.SystemLibrary.get_command_line() else 'apply.json')).write_text(json.dumps({'status':'PASS'}))
    except Exception:
        unreal.log_error(traceback.format_exc());raise
