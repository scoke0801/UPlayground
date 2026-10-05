"""Targeted content migration. -PGContentStep=1 or 2; unrelated rows/assets are preserved.
Run in a freshly built editor commandlet. Originals are copied once before the first save.
"""
import copy
import json
import os
import re
import shutil
import sys
import unreal
from datetime import datetime, timezone

ROOT = unreal.Paths.project_dir()
OUT = '/Game/DataCenter/ContentMilestone'
BACKUP = os.path.join(ROOT, 'Saved/Backups/ContentMilestone')
RUN_BACKUP = os.path.join(BACKUP, datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
match = re.search(r'PGContentStep=(\d+)', unreal.SystemLibrary.get_command_line())
STEP = int(match[1]) if match else 1
assert STEP in (1, 2)
tools = unreal.AssetToolsHelpers.get_asset_tools()

def preserve(path):
    relative = path.split('.')[0].removeprefix('/Game/') + '.uasset'
    source, target = os.path.join(ROOT, 'Content', relative), os.path.join(BACKUP, relative)
    if os.path.isfile(source) and not os.path.exists(target):
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copy2(source, target)
    current = os.path.join(RUN_BACKUP, relative)
    if os.path.isfile(source) and not os.path.exists(current):
        os.makedirs(os.path.dirname(current), exist_ok=True)
        shutil.copy2(source, current)

def save(asset):
    preserve(asset.get_path_name())
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

def rows(path):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))

def write(path, data):
    preserve(path)
    table = unreal.load_asset(path)
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(data, ensure_ascii=False))
    save(table)

def material():
    path = OUT + '/M_PatternBounds'
    if unreal.EditorAssetLibrary.does_asset_exist(path): return unreal.load_asset(path)
    mat = tools.create_asset('M_PatternBounds', OUT, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    lib = unreal.MaterialEditingLibrary
    def vector(name, value):
        node = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('default_value', unreal.LinearColor(*value))
        return node
    color = vector('GradeColor', (1,.18,.025,1))
    lib.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    nodes = {'P':lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition),
             'C':vector('Center',(0,0,0,1)), 'F':vector('Forward',(1,0,0,1))}
    for name, value in [('Radius',280),('Length',650),('HalfWidth',70),('Shape',0),('CosAngle',.42)]:
        node = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter)
        node.set_editor_property('parameter_name',name); node.set_editor_property('default_value',value)
        nodes[name] = node
    custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
    custom.set_editor_property('code', '''float2 d=P.xy-C.xy; float r=length(d); float along=dot(d,F.xy);
float side=abs(d.x*F.y-d.y*F.x); bool inside=Shape>1.5 ? along>=0 && along<=Length && side<=HalfWidth : r<=Radius && (Shape<0.5 || r<0.001 || along/r>=CosAngle);
float edge=Shape>1.5 ? min(min(along,Length-along),HalfWidth-side) : min(Radius-r,Shape>0.5 ? (along/max(r,0.001)-CosAngle)*r : Radius);
return inside ? (edge<8 ? 0.95 : 0.16) : 0;''')
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs=[]
    for name in nodes:
        inp=unreal.CustomInput(); inp.set_editor_property('input_name',name); inputs.append(inp)
    custom.set_editor_property('inputs',inputs)
    for name,node in nodes.items(): lib.connect_material_expressions(node,'',custom,name)
    lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(mat); save(mat)
    return mat

telegraph = material()
enemy_path = '/Game/DataCenter/DataTables/Actor/DT_Enemy'
skill_path = '/Game/DataCenter/DataTables/Skill/DT_Skill'
stage_path = '/Game/DataCenter/DataTables/Stage/DT_StageData'
enemies, skills, stages = rows(enemy_path), rows(skill_path), rows(stage_path)
original_enemies, original_skills = copy.deepcopy(enemies), copy.deepcopy(skills)
target_enemies = set(range(15101, 15106)) if STEP == 1 else {15106}
base = copy.deepcopy(next(r for r in skills if r['SkillID']==15104))
definitions = [
    (15101,'추격자의 베기','Sweep',.45,180,.5,2,220),
    (15102,'별빛 조준 사격','AimedProjectile',.8,80,.5,3,950),
    (15103,'수호자의 강타','Sweep',1.1,290,1.6,4,310),
    (15104,'분쇄자의 돌진 강타','ChargeSlam',1.1,260,1.7,5,850),
    (15105,'파수꾼의 위험 지대','HazardSequence',1.2,200,1.5,6,1000),
]
if STEP == 2:
    assert all(r.get('Role', 'Legacy') != 'Legacy' for r in enemies if r['EnemyID'] in range(15101,15106)), 'Apply step 1 first'
    definitions = [(15106,'황혼 횡베기','Sweep',.9,460,1.15,3,480),
                    (15107,'황혼 돌진 강타','ChargeSlam',1.25,350,1.8,6,1000),
                    (15108,'황혼 파동','HazardSequence',1.3,250,1.6,7,1200)]
for sid,title,pattern,windup,radius,recovery,cooldown,reach in definitions:
    existing = next((r for r in skills if r['SkillID']==sid), None)
    row = copy.deepcopy(existing if existing else base)
    if existing is None: row['Name'] = 'ContentPattern_'+str(sid)
    row.update(SkillID=sid,Desc=title,SkillType='Melee',Pattern=pattern,
               TelegraphDuration=windup,TelegraphRadius=radius,RecoveryDuration=recovery,SkillCoolTime=cooldown,SkillRange=reach,
               TelegraphMaterial=telegraph.get_path_name(),AimTrackingSeconds=windup*.35,HalfAngleDegrees=65 if sid!=15106 else 80,
               TravelDistance=1400 if pattern=='AimedProjectile' else 650,TravelSpeed=950 if pattern=='AimedProjectile' else 1100,
               LineHalfWidth=22 if pattern=='AimedProjectile' else 70,LandingTelegraphSeconds=.35,HazardCount=4 if sid==15108 else 3,
               HazardInterval=.6,HazardSpacing=220,MinimumBossPhase=2 if sid==15108 else 1,
               ProjectileClass='/Script/PGActor.PGPatternProjectile',AttackSound='/Game/DataCenter/CombatCycle/S_Heavy.S_Heavy')
    skills=[r for r in skills if r['SkillID']!=sid]+[row]
for row in enemies:
    eid=row['EnemyID']
    if eid not in target_enemies: continue
    row.update(Role={15101:'Chaser',15102:'Shooter',15103:'Guardian',15104:'Crusher',15105:'Warden',15106:'Boss'}[eid],
               PreferredDistance=700 if eid==15102 else 0,DistanceTolerance=120,GuardHalfAngle=70,GuardReduction=.7,
               TurnSpeed=95 if eid==15103 else 240,RetreatDistance=350,RetreatSeconds=.6,RetreatCooldown=1.2,
               SkillIdList=[15106,15107,15108] if eid==15106 else [eid],PhaseTwoHealthRatio=.5,PhaseTransitionSeconds=1.2,
               PhaseVFX='/Game/DataCenter/CombatCycle/NS_ImpactCritical.NS_ImpactCritical',PhaseSound='/Game/DataCenter/CombatCycle/S_Rare.S_Rare')
    if eid == 15106:
        row.update(PhaseTwoSkillSequence=[15108,15107,15106],
                   DefeatVFX='/Game/DataCenter/CombatCycle/NS_ImpactCritical.NS_ImpactCritical',
                   DefeatSound='/Game/DataCenter/CombatCycle/S_Rare.S_Rare',DefeatDisplaySeconds=3.)

# Keep the previous per-wave totals, changing composition rather than increasing combat load.
packs = {
 1:[[(15101,3),(15102,2)],[(15101,4),(15102,2)],[(15101,3),(15102,1)]],
 2:[[(15101,3),(15103,3)],[(15103,3),(15102,2),(15101,2)],[(15103,3),(15102,2),(15104,1)]],
 3:[[(15101,3),(15102,4)],[(15101,4),(15102,4)],[(15103,2),(15102,3)]],
 4:[[(15101,5),(15102,3)],[(15103,5),(15102,3),(15101,1)],[(15103,4),(15102,2),(15105,1)]],
 5:[[(15101,4),(15102,3),(15103,2)],[(15103,4),(15102,4),(15101,2)],[(15103,2),(15102,2),(15104,1),(15105,1)]]}
for stage in stages:
    if STEP != 1 or stage['Id'] not in packs: continue
    assert len(stage['Waves']) == len(packs[stage['Id']]), stage['Id']
    for wave,pack in zip(stage['Waves'],packs[stage['Id']]):
        total=sum(s['SpawnCount'] for s in wave['MonsterSpawnInfos'])
        assert sum(n for _,n in pack)==total, (stage['Id'],total,pack)
        wave['MonsterSpawnInfos']=[dict(MonsterId=eid,SpawnCount=n,SpawnPriority=0,SpawnDelayTime=0.) for eid,n in pack]
# Validate all compositions before saving any existing table or blueprint.
for row in enemies:
    if row['EnemyID'] not in target_enemies: continue
    bp_path=row['ActorClass'].split('.')[0]
    preserve(bp_path)
    bp=unreal.load_asset(bp_path)
    cls=unreal.EditorAssetLibrary.load_blueprint_class(bp_path)
    assert bp and cls, bp_path
    unreal.get_default_object(cls).set_editor_property('ai_controller_class',unreal.PGRoleAIController)
    save(bp)
target_skills = {definition[0] for definition in definitions}
assert [r for r in enemies if r['EnemyID'] not in target_enemies] == [r for r in original_enemies if r['EnemyID'] not in target_enemies]
assert [r for r in skills if r['SkillID'] not in target_skills] == [r for r in original_skills if r['SkillID'] not in target_skills]
write(skill_path,skills); write(enemy_path,enemies)
if STEP == 1:
    sys.path.insert(0, os.path.join(ROOT, 'Tools/Validation'))
    from P09WaveRoster import compose, P09_IDS
    from MonsterVariationRoster import compose as compose_variations, IDS as VARIATION_IDS
    registered={r['EnemyID'] for r in enemies}
    if VARIATION_IDS <= registered: stages=compose_variations(stages)
    elif P09_IDS <= registered: stages = compose(stages)
    write(stage_path,stages)
if STEP == 1:
    sys.path.insert(0, os.path.join(ROOT, 'Tools/Validation'))
    from ConfigureSkeletonArcher import apply as configure_skeleton_archer
    configure_skeleton_archer()
unreal.log('PGContent MIGRATION PASS step='+str(STEP))
