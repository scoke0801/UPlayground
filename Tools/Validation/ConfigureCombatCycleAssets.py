"""Combat cycle presentation and three-stage playable preset. Back up authored assets once."""
import unreal, os, json, shutil, math, wave, struct, random
ROOT = unreal.Paths.project_dir()
OUT = '/Game/DataCenter/CombatCycle'
BACKUP = os.path.join(unreal.Paths.project_saved_dir(), 'Backups', 'CombatCycle')
ART = os.path.join(ROOT, 'Tools', 'Art', 'CombatCycle')
tools = unreal.AssetToolsHelpers.get_asset_tools()
PREPARE_ONLY = 'PGPrepareOnly' in unreal.SystemLibrary.get_command_line()

def preserve(path):
    relative = path.split('.')[0].removeprefix('/Game/') + '.uasset'
    source = os.path.join(ROOT, 'Content', relative)
    target = os.path.join(BACKUP, relative)
    if os.path.isfile(source) and not os.path.exists(target):
        os.makedirs(os.path.dirname(target), exist_ok=True); shutil.copy2(source, target)

def save(asset):
    if PREPARE_ONLY and not asset.get_path_name().startswith(OUT + '/'):
        unreal.log('PGCombatCycle pending save: ' + asset.get_path_name()); return
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()

def load_rows(path):
    asset = unreal.load_asset(path)
    assert asset, path
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(asset))

def write_rows(path, rows):
    preserve(path); asset = unreal.load_asset(path)
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(asset, json.dumps(rows, ensure_ascii=False)), path
    save(asset)

def duplicate(source, name):
    path = OUT + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path): return unreal.load_asset(path)
    asset = unreal.EditorAssetLibrary.duplicate_asset(source, path)
    assert asset, source
    return asset

def import_file(filename, name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', filename); task.set_editor_property('destination_path', OUT)
    task.set_editor_property('destination_name', name); task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True); task.set_editor_property('save', True)
    tools.import_asset_tasks([task])
    asset = unreal.load_asset(OUT + '/' + name); assert asset, filename
    return asset

icon = import_file(os.path.join(ART, 'T_CombatReward.png'), 'T_CombatReward')
icon.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
icon.set_editor_property('max_texture_size', 512); save(icon)

perk_icon = import_file(os.path.join(ART, 'T_CombatPerks.png'), 'T_CombatPerks')
perk_icon.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
perk_icon.set_editor_property('max_texture_size', 2048); save(perk_icon)

# Small original synthesized impact and rarity cues; deterministic source WAVs remain reviewable.
os.makedirs(ART, exist_ok=True)
sounds = {}
for name, base, duration, metallic in [('Normal',110,.12,False),('Heavy',65,.22,False),('Critical',210,.18,True),('Common',440,.16,True),('Magic',660,.30,True),('Rare',880,.55,True)]:
    rng = random.Random(14); rate=44100; frames=[]
    for i in range(int(rate*duration)):
        t=i/rate; env=(1-t/duration)**2 * min(1,t/.003)
        value = (math.sin(2*math.pi*base*t) + .35*math.sin(2*math.pi*base*1.5*t) if metallic else .65*rng.uniform(-1,1)+math.sin(2*math.pi*(base*t-30*t*t)))
        frames.append(struct.pack('<h', int(max(-1,min(1,value*.28*env))*32767)))
    filename=os.path.join(ART,'S_'+name+'.wav')
    with wave.open(filename,'wb') as stream:
        stream.setnchannels(1); stream.setsampwidth(2); stream.setframerate(rate); stream.writeframes(b''.join(frames))
    sounds[name]=import_file(filename,'S_'+name)

# Finite single-instance camera patterns, authored as three separately tunable assets.
shakes={}
for name, duration, amplitude in [('Normal',.10,1.5),('Heavy',.16,3),('Critical',.12,2.3)]:
    path=OUT+'/CS_'+name
    bp=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if bp is None:
        factory=unreal.BlueprintFactory(); factory.set_editor_property('parent_class',unreal.PGCombatCameraShake)
        bp=tools.create_asset('CS_'+name,OUT,unreal.Blueprint,factory)
    cls=unreal.EditorAssetLibrary.load_blueprint_class(path); cdo=unreal.get_default_object(cls)
    pattern=cdo.get_editor_property('root_shake_pattern')
    pattern.set_editor_property('duration',duration); pattern.set_editor_property('amplitude',amplitude)
    cdo.set_editor_property('single_instance',True); save(bp); shakes[name]=cls

vfx={}
for name, source in [('Normal','/Game/ExternalAssets/VFX/MixedVFX/Particles/Projectiles/Hits/NS_Projectile_01_Hit'),('Heavy','/Game/ExternalAssets/VFX/MixedVFX/Particles/Explosions/NS_ExplosionGroundSmall'),('Critical','/Game/ExternalAssets/VFX/MixedVFX/Particles/Slashes/SeparateParts/Hits/NS_HolySlash_Hit')]:
    vfx[name]=duplicate(source,'NS_Impact'+name); save(vfx[name])
feedback_path='/Game/DataCenter/Stage12/DA_PGCombatFeedback'; preserve(feedback_path)
feedback=unreal.load_asset(feedback_path); assert feedback
for name, stop, intensity in [('Normal',.025,1),('Heavy',.05,1.2),('Critical',.04,1.4)]:
    entry=unreal.PGImpactFeedback()
    for key,value in [('vfx',vfx[name]),('sound',sounds[name]),('shake',shakes[name]),('hit_stop',stop),('intensity',intensity)]: entry.set_editor_property(key,value)
    feedback.set_editor_property(name.lower(),entry)
feedback.set_editor_property('maximum_hit_stop',.075); feedback.set_editor_property('camera_shake_interval',.18)
feedback.set_editor_property('critical_floater_scale',1.35); save(feedback)

# Analytical circle edge at UV radius .5 == authored world radius, no fuzzy exterior halo.
def material(name, decal):
    path=OUT+'/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path): return unreal.load_asset(path)
    mat=tools.create_asset(name,OUT,unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    if decal: mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL)
    else: mat.set_editor_property('two_sided',True)
    lib=unreal.MaterialEditingLibrary
    color=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,-300,-100)
    color.set_editor_property('parameter_name','GradeColor'); color.set_editor_property('default_value',unreal.LinearColor(1,.18,.025,1))
    lib.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if decal:
        uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-500,100)
        custom=lib.create_material_expression(mat,unreal.MaterialExpressionCustom,-250,100)
        custom.set_editor_property('code','float r = length(UV - float2(0.5,0.5)); return r <= 0.5 ? (r >= 0.46 ? 0.95 : 0.12) : 0.0;')
        custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        inp=unreal.CustomInput(); inp.set_editor_property('input_name','UV'); custom.set_editor_property('inputs',[inp])
        lib.connect_material_expressions(uv,'',custom,'UV')
        lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_OPACITY)
    else:
        opacity=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-250,100); opacity.set_editor_property('r',.32)
        lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(mat); save(mat); return mat
telegraph=material('M_EliteTelegraph',True); beam=material('M_LootBeam',False)

catalog_path='/Game/DataCenter/Progression/DA_PGProgression'; preserve(catalog_path)
catalog=unreal.load_asset(catalog_path); assert catalog
items=list(catalog.get_editor_property('items'))
for item in items: item.set_editor_property('icon',icon)
catalog.set_editor_property('items',items); catalog.set_editor_property('beam_material',beam)
catalog.set_editor_property('beam_heights',{unreal.PGItemRarity.COMMON:65.,unreal.PGItemRarity.MAGIC:125.,unreal.PGItemRarity.RARE:200.})
catalog.set_editor_property('drop_sounds',{unreal.PGItemRarity.COMMON:sounds['Common'],unreal.PGItemRarity.MAGIC:sounds['Magic'],unreal.PGItemRarity.RARE:sounds['Rare']}); save(catalog)

reward_path='/Game/DataCenter/DataTables/Reward/DT_StatReward'; rewards=load_rows(reward_path)
for i,row in enumerate(rewards): row.update(Grade=['Normal','Magic','Rare'][i%3],Icon=icon.get_path_name())
# Equal-grade alternatives, each changes a distinct combat decision.
perks=[('LifeSteal',8,'핏빛 서약','실제로 준 피해의 일부를 회복합니다.\n공격을 이어가며 버티세요.'),('Execution',40,'처형자의 칼날','생명력 35% 이하인 적에게 추가 피해.\n상처 입은 적부터 마무리하세요.'),('Counter',65,'파수꾼의 역습','정예의 강타 후 푸른 빈틈에 추가 피해.\n피한 뒤 즉시 반격하세요.')]
rewards=[r for r in rewards if r['StatId'] not in [14101,14102,14103]]
for i,(perk,amount,name,desc) in enumerate(perks):
    rewards.append(dict(Name='PG_Boon_'+perk,StatId=14101+i,DisplayName=name,StatType='Attack',Amount=0,Grade='Rare',Icon=perk_icon.get_path_name(),IconPanel=i,Perk=perk,PerkPercent=amount,PlaystyleDescription=desc))
write_rows(reward_path,rewards)

skill_path='/Game/DataCenter/DataTables/Skill/DT_Skill'; skills=load_rows(skill_path)
enemy_path='/Game/DataCenter/DataTables/Actor/DT_Enemy'; enemies=load_rows(enemy_path)
# Select one existing melee-only rig and clone it; all original enemies keep their patterns.
by_skill={row['SkillID']:row for row in skills}
base=next(row for row in enemies if row['EnemyID'] == 1001 and row.get('SkillIdList') and all(by_skill[s]['SkillType']=='Melee' for s in row['SkillIdList']))
source=base['ActorClass'].split("'")[-2] if "'" in base['ActorClass'] else base['ActorClass']
source=source.split('.')[0]
elite=duplicate(source,'BP_EliteWarden'); cls=unreal.EditorAssetLibrary.load_blueprint_class(OUT+'/BP_EliteWarden')
cdo=unreal.get_default_object(cls); cdo.set_editor_property('character_tid',14001); cdo.set_editor_property('feedback_data',feedback); save(elite)
row=dict(by_skill[base['SkillIdList'][0]])
montage_source=row['MontagePath']['AssetPath']['PackageName']
montage=duplicate(montage_source,'AM_EliteSlam_Presentation')
unreal.AnimationLibrary.remove_all_animation_notify_tracks(montage); save(montage)
row['ElitePresentationMontage']=montage.get_path_name()
row.update(Name='PG_EliteSlam',SkillID=14001,Desc='정예 파수꾼 강타',SkillRange=500.,SkillCoolTime=5,TelegraphDuration=1.1,RecoveryDuration=1.65,RecoveryDamageBonus=.35,TelegraphRadius=280.,TelegraphMaterial=telegraph.get_path_name(),SlamVFX=vfx['Heavy'].get_path_name())
skills=[r for r in skills if r['SkillID']!=14001]+[row]; write_rows(skill_path,skills)
row=dict(base); row.update(Name='PG_EliteWarden',EnemyID=14001,EnemyName='정예 파수꾼',SkillIdList=[14001],ActorClass=cls.get_path_name())
write_rows(enemy_path,[r for r in enemies if r['EnemyID']!=14001]+[row])
stat_path='/Game/DataCenter/DataTables/Actor/DT_CharacterStat'; stats=load_rows(stat_path)
row=dict(next(r for r in stats if r['CharacterID']==base['EnemyID'])); row.update(Name='PG_EliteWarden',CharacterID=14001)
row['Stats']=dict(row['Stats'])
for key,minimum,multiplier in [('Health',4000,3),('Attack',200,2)]:
    old=row['Stats'].get(key,0)
    row['Stats'][key]=max(minimum,(old.get('Stats',0) if isinstance(old,dict) else old)*multiplier)
stat_rows=[r for r in stats if r['CharacterID']!=14001]+[row]
for stat_row in stat_rows:
    stat_row['Stats']={key:(value.get('Stats',0) if isinstance(value,dict) else value) for key,value in stat_row['Stats'].items()}
write_rows(stat_path,stat_rows)
restored_stats={r['CharacterID']:r for r in load_rows(stat_path)}
for expected in stat_rows:
    actual=restored_stats[expected['CharacterID']]['Stats']
    actual={k:(v.get('Stats',0) if isinstance(v,dict) else v) for k,v in actual.items()}
    assert actual==expected['Stats'], 'Stat roundtrip failed: '+str(expected['CharacterID'])
death_path='/Game/DataCenter/DataTables/Path/DT_Death'; deaths=load_rows(death_path)
base_death=next((r for r in deaths if r['ObjectTID']==base['EnemyID']),None)
if base_death:
    row=dict(base_death); row.update(Name='PG_EliteWarden',ObjectTID=14001); write_rows(death_path,[r for r in deaths if r['ObjectTID']!=14001]+[row])
stage_path='/Game/DataCenter/DataTables/Stage/DT_StageData'; stages=load_rows(stage_path)
for stage_index,stage in enumerate(sorted(stages,key=lambda r:r['Id'])[:3]):
    stage.update(StageName='파수꾼의 회랑 '+str(stage['Id']),SpawnBatchSize=3,SpawnInterval=1.,NextStageDelay=3.)
    stage['RewardPool']=[dict(RewardType='Stat',RewardId=14101+i,Weight=1.) for i in range(3)]
    stage['MonsterSpawnInfos']=[dict(MonsterId=base['EnemyID'],SpawnCount=n,SpawnPriority=0,SpawnDelayTime=delay) for n,delay in [(3+stage_index,2.),(4+stage_index,12.),(4+stage_index,22.)]]+[dict(MonsterId=14001,SpawnCount=1,SpawnPriority=1,SpawnDelayTime=32.)]
write_rows(stage_path,stages)
unreal.log('PGCombatCycle ' + ('PREPARED; existing asset saves still pending' if PREPARE_ONLY else 'ASSETS COMPLETE') + ' elite_base='+str(base['EnemyID']))






