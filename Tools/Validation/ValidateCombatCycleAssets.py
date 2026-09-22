"""Read-only validation of combat-cycle content after ConfigureCombatCycleAssets.py."""
import unreal, json, os
OUT='/Game/DataCenter/CombatCycle'

def rows(path):
    asset=unreal.load_asset(path); assert asset,path
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(asset))
feedback=unreal.load_asset('/Game/DataCenter/Stage12/DA_PGCombatFeedback'); assert feedback
for kind in ['normal','heavy','critical']:
    entry=feedback.get_editor_property(kind)
    for field in ['vfx','sound','shake']: assert entry.get_editor_property(field), (kind,field)
    assert 0 < entry.get_editor_property('hit_stop') <= feedback.get_editor_property('maximum_hit_stop')
    shake=unreal.get_default_object(entry.get_editor_property('shake'))
    assert shake.get_editor_property('single_instance')
    assert shake.get_editor_property('root_shake_pattern').get_editor_property('duration') <= feedback.get_editor_property('camera_shake_interval')
skills=rows('/Game/DataCenter/DataTables/Skill/DT_Skill'); elite=next(r for r in skills if r['SkillID']==14001)
assert elite['TelegraphDuration']>=.8 and elite['RecoveryDuration']>=1 and elite['TelegraphRadius']>0
assert unreal.load_asset(elite['TelegraphMaterial']) and unreal.load_asset(elite['SlamVFX'])
assert unreal.load_asset(elite['ElitePresentationMontage'])
enemies=rows('/Game/DataCenter/DataTables/Actor/DT_Enemy'); enemy=next(r for r in enemies if r['EnemyID']==14001)
assert enemy['SkillIdList']==[14001]
cls=unreal.EditorAssetLibrary.load_blueprint_class(OUT+'/BP_EliteWarden')
assert unreal.get_default_object(cls).get_editor_property('character_tid')==14001
stages=rows('/Game/DataCenter/DataTables/Stage/DT_StageData')
for stage in sorted(stages,key=lambda r:r['Id'])[:3]:
    waves=stage['Waves']; assert len(waves)==4
    spawns=[spawn for wave in waves for spawn in wave['MonsterSpawnInfos']]
    assert len([s for s in spawns if s['MonsterId']==14001 and s['SpawnCount']==1])==1
    assert waves[-1]['MonsterSpawnInfos'][0]['MonsterId']==14001
    assert stage['BuildDuration']==30.
catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
assert catalog.get_editor_property('beam_material')
assert len(catalog.get_editor_property('drop_sounds'))==3
for item in catalog.get_editor_property('items'): assert item.get_editor_property('icon')
for row in rows('/Game/DataCenter/DataTables/Reward/DT_StatReward'): assert unreal.load_asset(row['Icon'])
boons=[r for r in rows('/Game/DataCenter/DataTables/Reward/DT_StatReward') if r['StatId'] in [14101,14102,14103]]
assert len(boons)==3 and {r['Perk'] for r in boons}=={'LifeSteal','Execution','Counter'}
assert {r['IconPanel'] for r in boons}=={0,1,2}
assert all(r['Amount']==0 and r['PerkPercent']>0 and r['PlaystyleDescription'] for r in boons)
assert elite['RecoveryDamageBonus']>0
for stage in sorted(stages,key=lambda r:r['Id'])[:3]:
    assert {r['RewardId'] for r in stage['RewardPool']}=={14101,14102,14103}
    assert all(w['StartDelay']>=0 for w in stage['Waves'])
prepared='PGPrepareOnly'  in unreal.SystemLibrary.get_command_line()
report=dict(result='prepared_in_memory' if prepared else 'passed',stages=3,elite=14001,feedback=['normal','heavy','critical'],items=len(catalog.get_editor_property('items')))
folder=os.path.join(unreal.Paths.project_saved_dir(),'Automation','CombatCycle'); os.makedirs(folder,exist_ok=True)
with open(os.path.join(folder,'AssetsPrepared.json' if prepared else 'Assets.json'),'w',encoding='utf-8') as stream: json.dump(report,stream,ensure_ascii=False,indent=2)
unreal.log('PGCombatCycle VALIDATION PASSED '+str(report))

