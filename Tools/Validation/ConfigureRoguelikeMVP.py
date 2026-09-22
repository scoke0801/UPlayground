"""Author the approved MVP. Backs up every replaced asset once; rerunnable.
Run with UE 5.8 PythonScript commandlet after compiling the project.
"""
import unreal, os, json, copy, shutil
ROOT = unreal.Paths.project_dir()
OUT = '/Game/DataCenter/RoguelikeMVP'
BACKUP = os.path.join(ROOT, 'Saved/Backups/RoguelikeMVP')
tools = unreal.AssetToolsHelpers.get_asset_tools()

def preserve(path):
    relative = path.split('.')[0].removeprefix('/Game/') + '.uasset'
    source, target = os.path.join(ROOT,'Content',relative), os.path.join(BACKUP,relative)
    if os.path.isfile(source) and not os.path.exists(target):
        os.makedirs(os.path.dirname(target),exist_ok=True); shutil.copy2(source,target)

def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()

def rows(path):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))

def write(path, data):
    preserve(path)
    table = unreal.load_asset(path)
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(data,ensure_ascii=False)),path
    save(table)

def duplicate(source, name):
    target = OUT+'/'+name
    asset = unreal.load_asset(target) if unreal.EditorAssetLibrary.does_asset_exist(target) else unreal.EditorAssetLibrary.duplicate_asset(source,target)
    assert asset, source
    return asset

# Reuse the existing validated telegraph/material/SFX authoring pipeline in its own namespace.
for path in ['/Game/DataCenter/DataTables/Skill/DT_Skill','/Game/DataCenter/DataTables/Actor/DT_Enemy',
             '/Game/DataCenter/DataTables/Actor/DT_CharacterStat','/Game/DataCenter/DataTables/Stage/DT_StageData',
             '/Game/DataCenter/DataTables/Reward/DT_StatReward','/Game/DataCenter/DataTables/Path/DT_Death',
             '/Game/DataCenter/Stage12/DA_PGCombatFeedback','/Game/DataCenter/Progression/DA_PGProgression']:
    preserve(path)
namespace = {'__name__':'__main__'}
exec(compile(open(os.path.join(ROOT,'Tools/Validation/ConfigureCombatCycleAssets.py'),encoding='utf-8-sig').read(),'ConfigureCombatCycleAssets.py','exec'),namespace)

task = unreal.AssetImportTask()
for key,value in dict(filename=os.path.join(ROOT,'Tools/Art/RoguelikeMVP/T_BuildAtlas.png'),destination_path=OUT,destination_name='T_BuildAtlas',automated=True,replace_existing=True,save=True).items():
    task.set_editor_property(key,value)
tools.import_asset_tasks([task])
icon = unreal.load_asset(OUT+'/T_BuildAtlas'); assert icon
icon.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
icon.set_editor_property('max_texture_size',2048); save(icon)

# Each effect is implemented by the ASC; values are percentages except explicit stat rewards.
definitions = [
 ('장미의 상처','Bleed',12,'None','Bleed',0,'적중 시 3초간 출혈. 0.5초마다 피해, 최대 5중첩.'),
 ('깊은 흉터','BleedPotency',50,'Bleed','Bleed',0,'출혈 틱 피해 +50%.'),
 ('붉은 개화','BleedSpread',50,'Bleed','Bleed',0,'출혈 중인 적 처치 시 주변 적에게 출혈 확산.'),
 ('핏빛 종결','BleedBurst',100,'Bleed','Bleed',0,'액티브 적중 시 남은 출혈 피해를 즉시 폭발.'),
 ('공명의 칼날','Shockwave',40,'None','Shock',1,'액티브 적중 시 주변에 충격파. 발동 간격 0.8초.'),
 ('넓어지는 파문','ShockRadius',40,'Shockwave','Shock',1,'충격파 반경 +40%.'),
 ('메아리','ShockEcho',65,'Shockwave','Shock',1,'0.3초 뒤 원래 파동 피해의 65%로 추가 충격파.'),
 ('분쇄의 울림','ShockExecute',75,'Shockwave','Shock',1,'체력 35% 이하 적에게 충격파 피해 +75%.'),
 ('격분','Frenzy',5,'None','Frenzy',2,'연속 적중마다 공격 속도 +5%. 최대 10중첩, 3초 유지.'),
 ('꺼지지 않는 불','FrenzyDuration',100,'Frenzy','Frenzy',2,'격분 유지 시간 +100%.'),
 ('전투의 갈증','FrenzyLeech',5,'Frenzy','Frenzy',2,'격분을 쌓는 직접 공격 피해의 5%만큼 회복.'),
 ('불굴의 투지','FrenzyGuard',25,'Frenzy','Frenzy',2,'격분 중 받는 직접 공격 피해 25% 감소.'),
 ('흡혈','LifeSteal',3,'None','None',0,'직접 공격으로 실제 가한 피해의 3% 회복.'),
 ('신속한 순환','Cooldown',15,'None','None',1,'스킬 쿨다운 15% 감소.'),
 ('날카로운 칼날','None',0,'None','None',2,'공격력 +55.'),
 ('별의 가호','None',0,'None','None',1,'최대 생명력 +250. 현재 생명력은 별도로 회복되지 않습니다.'),
 ('단단한 결의','None',0,'None','None',2,'방어력 +40.'),
 ('행운의 일격','None',0,'None','None',0,'치명타 확률 +5%p.'),
]
reward_path='/Game/DataCenter/DataTables/Reward/DT_StatReward'
rewards=[r for r in rows(reward_path) if not 15000 <= r['StatId'] < 15100]
for i,(name,perk,pct,required,family,panel,description) in enumerate(definitions):
    stat,amount={14:('Attack',55),15:('Health',250),16:('Defense',40),17:('CriticalRate',500)}.get(i,('Attack',0))
    rewards.append(dict(Name='Rogue_'+str(i),StatId=15000+i,DisplayName=name,StatType=stat,Amount=amount,
        Grade='Rare' if required!='None' else 'Magic',Icon=icon.get_path_name(),Perk=perk,PerkPercent=pct,
        PlaystyleDescription=description,IconPanel=panel,RequiredPerk=required,BuildFamily=family,MaxSelections=1 if i<12 else 2))
write(reward_path,rewards)

catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
catalog.set_editor_property('roguelike_runs',True)
catalog.set_editor_property('drop_chance',.08)
catalog.set_editor_property('starting_items',[15001])
items=[]
item_specs=[(15001,'새벽의 검','WEAPON','COMMON',25,5,{}),
 (15002,'푸른 강철','WEAPON','MAGIC',65,4,{}),
 (15003,'수호의 별','ACCESSORY','MAGIC',25,4,{}),
 (15004,'전설 · 붉은 장미','WEAPON','RARE',100,1,{'BLEED':12,'BLEED_SPREAD':50}),
 (15005,'전설 · 공명의 심장','ACCESSORY','RARE',45,1,{'SHOCKWAVE':40,'SHOCK_ECHO':65}),
 (15006,'전설 · 불멸의 늑대','WEAPON','RARE',100,1,{'FRENZY':5,'FRENZY_DURATION':100})]
for iid,name,slot,rarity,power,weight,perks in item_specs:
    item=unreal.PGItemDataRow()
    for key,value in dict(id=iid,display_name=name,slot=getattr(unreal.PGEquipmentSlot,slot),rarity=getattr(unreal.PGItemRarity,rarity),
        base_options={unreal.PGStatType.ATTACK:power},roll_bonus=10,drop_weight=weight,
        combat_perks={getattr(unreal.PGCombatPerk,k):v for k,v in perks.items()},icon=icon,
        effect_description={15004:'출혈 부여 · 처치 시 확산',15005:'충격파 발동 · 추가 메아리',15006:'격분 부여 · 지속 시간 증가'}.get(iid,'')).items(): item.set_editor_property(key,value)
    items.append(item)
catalog.set_editor_property('items',items)
builds=[]
for name,title,clears,first,second in [('Dawn','새벽 검술',0,114,112),('Rose','장미 검술',3,111,113)]:
    build=unreal.PGBuildDefinition(); build.set_editor_property('id',name); build.set_editor_property('display_name',title); build.set_editor_property('required_clears',clears)
    entries=[]
    for slot,sid,cd in [('NORMAL_ATTACK',100,-1),('SKILL_SLOT_1',first,5),('SKILL_SLOT_2',second,8),('SKILL_SLOT_ROLL',10000,1.2)]:
        entry=unreal.PGLoadoutEntry(); entry.set_editor_property('slot',getattr(unreal.PGSkillSlot,slot)); entry.set_editor_property('skill_id',sid); entry.set_editor_property('cooldown_seconds',cd); entries.append(entry)
    build.set_editor_property('skills',entries); builds.append(build)
catalog.set_editor_property('builds',builds); save(catalog)

enemy_path='/Game/DataCenter/DataTables/Actor/DT_Enemy'
skill_path='/Game/DataCenter/DataTables/Skill/DT_Skill'
stat_path='/Game/DataCenter/DataTables/Actor/DT_CharacterStat'
death_path='/Game/DataCenter/DataTables/Path/DT_Death'
enemies,skills,stats,deaths=rows(enemy_path),rows(skill_path),rows(stat_path),rows(death_path)
for collection,key in [(enemies,'EnemyID'),(skills,'SkillID'),(stats,'CharacterID'),(deaths,'ObjectTID')]:
    collection[:]=[r for r in collection if not 15101<=r[key]<=15110]
def softpath(value):
    if isinstance(value,str): return value
    a=value['AssetPath']; return a['PackageName']+'.'+a['AssetName']

# Three normal roles and two telegraphed elite variants; final boss has a wide, slower slam.
specs=[(15101,1001,'추격자',1300,80,35,390,None),
       (15102,1003,'별빛 사수',950,100,20,310,None),
       (15103,1002,'철갑 수호자',2300,140,110,230,None),
       (15104,1002,'정예 · 분쇄자',5500,220,60,300,(1.1,250,1.5,4)),
       (15105,1002,'정예 · 파수꾼',7000,240,80,270,(1.5,430,1.8,5)),
       (15106,1002,'황혼의 기사',28000,320,90,290,(1.35,480,1.65,3.8))]
for eid,baseid,title,hp,attack,defense,speed,slam in specs:
    base=next(r for r in enemies if r['EnemyID']==baseid)
    bp=duplicate(base['ActorClass'].split('.')[0],'BP_Rogue_'+str(eid))
    cls=unreal.EditorAssetLibrary.load_blueprint_class(OUT+'/BP_Rogue_'+str(eid))
    cdo=unreal.get_default_object(cls); cdo.set_editor_property('character_tid',eid)
    if eid==15106: cdo.get_editor_property('mesh').set_editor_property('relative_scale3d',unreal.Vector(1.65,1.65,1.65))
    save(bp)
    enemy=copy.deepcopy(base); enemy.update(Name='Rogue_'+str(eid),EnemyID=eid,EnemyName=title,ActorClass=cls.get_path_name())
    if slam:
        skill=copy.deepcopy(next(r for r in skills if r['SkillID']==14001))
        skill.update(Name='RogueSlam_'+str(eid),SkillID=eid,TelegraphDuration=slam[0],TelegraphRadius=slam[1],RecoveryDuration=slam[2],SkillCoolTime=slam[3],SkillRange=600.)
        skills.append(skill); enemy['SkillIdList']=[eid]
    enemies.append(enemy)
    stat=copy.deepcopy(next(r for r in stats if r['CharacterID']==baseid)); stat.update(Name='Rogue_'+str(eid),CharacterID=eid)
    stat['Stats']={k:(v.get('Stats',0) if isinstance(v,dict) else v) for k,v in stat['Stats'].items()}
    stat['Stats'].update(Health=hp,Attack=attack,Defense=defense,MovementSpeed=speed); stats.append(stat)
    base_death=next((r for r in deaths if r['ObjectTID']==baseid),None)
    if base_death:
        death=copy.deepcopy(base_death); death.update(Name='Rogue_'+str(eid),ObjectTID=eid); deaths.append(death)
for stat in stats:
    stat['Stats']={k:(v.get('Stats',0) if isinstance(v,dict) else v) for k,v in stat['Stats'].items()}
    if stat['CharacterID']==1: stat['Stats'].update(Health=1800,Attack=320,Defense=100)
for path,data in [(enemy_path,enemies),(skill_path,skills),(stat_path,stats),(death_path,deaths)]: write(path,data)

stage_path='/Game/DataCenter/DataTables/Stage/DT_StageData'
template=rows(stage_path)[0]; stages=[]
names=['새벽의 문','부서진 회랑','별빛 정원','침묵의 성채','황혼의 경계','황혼의 기사']
for index,title in enumerate(names,1):
    stage=copy.deepcopy(template)
    stage.update(Name='RogueStage_'+str(index),Id=index,StageName=title,bIsBossStage=index==6,bManualReady=True,RewardSelections=2 if index in (2,4) else 1,SpawnBatchSize=3,SpawnInterval=1.3,SpawnRadius=1200.)
    waves=[]
    packs=[[(15101,4+index)],[(15101,3+index),(15102,2)],[(15103,2+index//2),(15102,2)]] if index<6 else [[(15106,1)]]
    if index in (2,4): packs[-1].append((15104 if index==2 else 15105,1))
    for wi,pack in enumerate(packs):
        waves.append(dict(StartDelay=2. if wi else 0.,MonsterSpawnInfos=[dict(MonsterId=eid,SpawnCount=count,SpawnPriority=0,SpawnDelayTime=0.) for eid,count in pack]))
    stage['Waves']=waves; stage['MonsterSpawnInfos']=[]
    ids=[15000,15004,15008] if index==1 else list(range(15000,15018)) if index<6 else []
    stage['RewardPool']=[dict(RewardType='Stat',RewardId=i,Weight=1.) for i in ids]; stages.append(stage)
write(stage_path,stages)

tuning=unreal.load_asset('/Game/DataCenter/Stage12/DA_PGCombatTuning')
if tuning:
    preserve(tuning.get_path_name())
    for key,name in [('bleed_vfx','NS_ImpactNormal'),('shock_vfx','NS_ImpactHeavy'),('frenzy_vfx','NS_ImpactCritical')]:
        tuning.set_editor_property(key,unreal.load_asset('/Game/DataCenter/CombatCycle/'+name))
    save(tuning)
os.makedirs(os.path.join(ROOT,'Saved/RoguelikeMVP'),exist_ok=True)
with open(os.path.join(ROOT,'Saved/RoguelikeMVP/manifest.json'),'w',encoding='utf-8') as f:
    json.dump(dict(stages=stages,rewards=rewards[-18:],enemies=specs,art=icon.get_path_name()),f,ensure_ascii=False,indent=2)
unreal.log('PGRogue ASSETS COMPLETE stages=6 rewards=18 legendary=3 builds=3')
