"""Step 3 only: preserve existing balance, waves, items and unrelated table rows."""
import copy
import json
import os
import shutil
from datetime import datetime, timezone
import unreal

ROOT = unreal.Paths.project_dir()
BACKUP = os.path.join(ROOT, 'Saved/Backups/BuildKeystones', datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))

def preserve(path):
    relative = path.split('.')[0].removeprefix('/Game/') + '.uasset'
    source = os.path.join(ROOT, 'Content', relative)
    target = os.path.join(BACKUP, relative)
    if os.path.isfile(source) and not os.path.exists(target):
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copy2(source, target)

def rows(path):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))

def write(path, data):
    preserve(path)
    table = unreal.load_asset(path)
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(data, ensure_ascii=False))
    assert unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False)

reward_path = '/Game/DataCenter/DataTables/Reward/DT_StatReward'
stage_path = '/Game/DataCenter/DataTables/Stage/DT_StageData'
rewards, stages = rows(reward_path), rows(stage_path)
before_rewards, before_stages = copy.deepcopy(rewards), copy.deepcopy(stages)
by_id = {r['StatId']: r for r in rewards}
assert all(i in by_id for i in range(15000,15018)), 'Apply the MVP before step 3'
# Correct the proc condition wording without resetting the designer's existing values.
by_id[15002]['PlaystyleDescription'] = '출혈 중인 적 처치 시 현재 출혈 틱 피해에 확산 비율을 곱해 주변에 출혈을 부여합니다.'
by_id[15003]['PlaystyleDescription'] = '액티브 직접 적중 시 남은 출혈을 폭발시키고 생존한 대상에 새 출혈을 부여합니다.'
by_id[15013]['PlaystyleDescription'] = '장착 스킬과 회피의 쿨다운을 줄입니다. 최대 50% 감소.'
definitions = [
    (15018,15000,'핏빛 순환','BleedRecast','Bleed',['BleedBurst'],[], '출혈 폭발 처치 시 사용한 액티브의 남은 쿨다운 일부 반환. 스킬 사용당 1회.'),
    (15019,15004,'공명 균열','ShockFracture','Shockwave',[],['ShockRadius','ShockEcho','ShockExecute'], '같은 적에게 충격파와 메아리를 누적 적중하면 잠시 방어력이 감소합니다.'),
    (15020,15008,'격분의 잔상','FrenzyAfterimage','Frenzy',[],['FrenzyDuration','FrenzyLeech','FrenzyGuard'], '최대 격분에서 회피하면 중첩을 모두 소비하고 출발점에 잔상 타격을 남깁니다.'),
]
for rid,root,title,perk,required,all_of,any_of,desc in definitions:
    row = copy.deepcopy(by_id.get(rid,by_id[root]))
    row.update(Name='Keystone_'+str(rid),StatId=rid,DisplayName=title,Perk=perk,PerkPercent=1,Amount=0,
               Grade='Rare',RequiredPerk=required,RequiredPerks=all_of,RequiredAnyPerks=any_of,
               bKeystone=True,MaxSelections=1,PlaystyleDescription=desc)
    rewards = [r for r in rewards if r['StatId'] != rid] + [row]
for stage in stages:
    if stage['Id'] not in (4,5): continue
    stage['bReserveKeystoneChoice'] = True
    for rid in (15018,15019,15020):
        if not any(r['RewardId']==rid for r in stage['RewardPool']):
            stage['RewardPool'].append(dict(RewardType='Stat',RewardId=rid,Weight=1.))
assert [r for r in rewards if not 15000<=r['StatId']<=15020] == [r for r in before_rewards if not 15000<=r['StatId']<=15020]
for previous,current in zip(before_stages,stages):
    a,b=copy.deepcopy(previous),copy.deepcopy(current)
    for key in ('RewardPool','bReserveKeystoneChoice'): a.pop(key,None); b.pop(key,None)
    assert a==b, 'Stage behavior changed'
assert sum(r['RewardSelections'] for r in stages if not r['bIsBossStage'])==7
tuning = unreal.load_asset('/Game/DataCenter/Stage12/DA_PGCombatTuning')
assert tuning
preserve(tuning.get_path_name())
tuning.set_editor_property('afterimage_vfx', unreal.load_asset('/Game/DataCenter/CombatCycle/NS_ImpactCritical'))
write(reward_path,rewards)
write(stage_path,stages)
assert unreal.EditorAssetLibrary.save_loaded_asset(tuning,only_if_is_dirty=False)
unreal.log('PGBuildKeystones MIGRATION PASS rewards=21 backup='+BACKUP)
