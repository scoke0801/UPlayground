"""Read-only cross-reference and required-asset validation in Unreal."""
import json
import math
import unreal
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
errors = []
tables = {}
for item in registry.get_assets_by_path("/Game/DataCenter/DataTables", recursive=True):
    table = item.get_asset()
    if isinstance(table, unreal.DataTable):
        tables[str(item.asset_name)] = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
def indexed(name, key):
    out = {}
    if not tables.get(name):
        errors.append(f'{name}: required table is missing or empty')
    for row in tables.get(name, []):
        value = row[key]
        if value in out or value <= 0:
            errors.append(f"{name}/{row['Name']}: invalid or duplicate {key}={value}")
        out[value] = row
    return out
skills = indexed("DT_Skill", "SkillID")
enemies = indexed("DT_Enemy", "EnemyID")
rewards = indexed("DT_StatReward", "StatId")
stages = indexed("DT_StageData", "Id")
for skill in skills.values():
    if skill['SkillCoolTime'] < 0 or skill['SkillRange'] < 0:
        errors.append(f"Skill {skill['SkillID']}: negative cooldown/range")
    for child in skill['ChainSkillIdList']:
        if child not in skills:
            errors.append(f"Skill {skill['SkillID']}: missing chain {child}")
    # Chain lists are finite combo sequences, not recursive graph edges.
    montage = skill['MontagePath']['AssetPath']
    if str(montage['PackageName']) != 'None' and not unreal.load_asset(montage['PackageName']):
        errors.append(f"Skill {skill['SkillID']}: missing montage")
for reward in rewards.values():
    if reward['Amount'] <= 0 or reward['StatType'] in ('None', 'Max'):
        errors.append(f"Invalid stat reward: {reward['StatId']}")
for enemy in enemies.values():
    if not unreal.load_class(None, enemy['ActorClass']):
        errors.append(f"Enemy {enemy['EnemyID']}: class cannot be loaded")
    for skill_id in enemy['SkillIdList']:
        if skill_id not in skills:
            errors.append(f"Enemy {enemy['EnemyID']}: missing skill {skill_id}")
for stage in stages.values():
    if stage['MaxSpawnRetries'] <= 0 or stage['SpawnRadius'] < 100 or not math.isfinite(stage['SpawnInterval']):
        errors.append(f"Stage {stage['Id']}: invalid retry/radius/interval")
    if sum(spawn['SpawnCount'] for spawn in stage['MonsterSpawnInfos']) > 2147483647:
        errors.append(f"Stage {stage['Id']}: monster count overflow")
    if stage['SpawnBatchSize'] <= 0 or stage['SpawnInterval'] < 0 or not stage['MonsterSpawnInfos']:
        errors.append(f"Stage {stage['Id']}: invalid spawn schedule")
    for spawn in stage['MonsterSpawnInfos']:
        if spawn['MonsterId'] not in enemies or spawn['SpawnCount'] <= 0 or spawn['SpawnDelayTime'] < 0:
            errors.append(f"Stage {stage['Id']}: invalid spawn {spawn}")
    for reward in stage['RewardPool']:
        if reward['RewardType'] != 'Stat' or reward['RewardId'] not in rewards or reward['Weight'] <= 0:
            errors.append(f"Stage {stage['Id']}: unsupported/missing reward {reward}")
for error in errors:
    unreal.log_error(error)
if errors:
    raise RuntimeError(f"PG validation failed: {len(errors)} errors")
unreal.log(f"PG validation passed: {len(stages)} stages, {len(enemies)} enemies, {len(skills)} skills, {len(rewards)} stat rewards")