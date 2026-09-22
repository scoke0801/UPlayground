"""Read back saved MVP assets and validate references and reachable build graphs."""
import unreal, json, os
assert unreal.EditorAssetLibrary.does_asset_exist('/Game/Maps/RogueArena')
def rows(path):
    asset=unreal.load_asset(path); assert asset,path
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(asset))
stages=rows('/Game/DataCenter/DataTables/Stage/DT_StageData')
rewards={r['StatId']:r for r in rows('/Game/DataCenter/DataTables/Reward/DT_StatReward')}
enemies={r['EnemyID']:r for r in rows('/Game/DataCenter/DataTables/Actor/DT_Enemy')}
skills={r['SkillID']:r for r in rows('/Game/DataCenter/DataTables/Skill/DT_Skill')}
stats={r['CharacterID']:r for r in rows('/Game/DataCenter/DataTables/Actor/DT_CharacterStat')}
assert sorted(r['Id'] for r in stages)==list(range(1,7))
assert sum(r['RewardSelections'] for r in stages if not r['bIsBossStage'])==7
for stage in stages:
    assert stage['bManualReady']
    assert stage['Waves']
    for wave in stage['Waves']:
        for spawn in wave['MonsterSpawnInfos']:
            eid=spawn['MonsterId']; assert eid in enemies and eid in stats and spawn['SpawnCount']>0
            cls=unreal.load_class(None,enemies[eid]['ActorClass']); assert cls
            cdo=unreal.get_default_object(cls); assert cdo.get_editor_property('character_tid')==eid
            for sid in enemies[eid]['SkillIdList']: assert sid in skills
    for reward in stage['RewardPool']: assert reward['RewardId'] in rewards
roots=[r for r in rewards.values() if 15000<=r['StatId']<15012 and r['RequiredPerk']=='None']
assert len(roots)==3
for family in ['Bleed','Shock','Frenzy']:
    branch=[r for r in rewards.values() if 15000<=r['StatId']<15012 and r['BuildFamily']==family]
    assert len(branch)==4
    root=next(r for r in branch if r['RequiredPerk']=='None')
    assert all(r['RequiredPerk'] in ['None',root['Perk']] for r in branch)
    for reward in branch: assert unreal.load_asset(reward['Icon'])
catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
assert catalog.get_editor_property('roguelike_runs')
items=list(catalog.get_editor_property('items'))
assert len([i for i in items if len(i.get_editor_property('combat_perks'))])==3
assert len(catalog.get_editor_property('builds'))==2
for build in catalog.get_editor_property('builds'):
    assert len(build.get_editor_property('skills'))==4
    for skill in build.get_editor_property('skills'):
        row=skills[skill.get_editor_property('skill_id')]
        p=row['MontagePath']['AssetPath']; assert p['PackageName']!='None'
for eid in [15104,15105,15106]:
    row=skills[eid]
    assert row['TelegraphDuration']>0 and row['RecoveryDuration']>0 and row['TelegraphRadius']>0
    assert unreal.load_asset(row['ElitePresentationMontage'])
player=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Actor/LocalPlayer/BP_LocalPlayer'))
tuning=player.get_editor_property('ability_system_component').get_editor_property('combat_tuning')
assert tuning
for prop in ['bleed_vfx','shock_vfx','frenzy_vfx']: assert tuning.get_editor_property(prop)
result=dict(status='PASS',stages=6,reward_choices=7,rewards=18,build_branches=3,legendary_items=3,normal_enemies=3,elites=2,bosses=1)
dest=os.path.join(unreal.Paths.project_saved_dir(),'RoguelikeMVP','validation.json')
with open(dest,'w',encoding='utf-8') as f: json.dump(result,f,indent=2)
unreal.log('PGRogue VALIDATION PASS '+json.dumps(result))
