import unreal, json
for path in ['/Game/Blueprints/Actor/NonPlayer/Enemy/Ability/GA_EnemyAbility_MeleeSkill','/Game/DataCenter/DataTables/Skill/DT_Skill','/Game/DataCenter/DataTables/Actor/DT_Enemy','/Game/DataCenter/DataTables/Stage/DT_StageData']:
 a=unreal.load_asset(path)
 if isinstance(a,unreal.DataTable): unreal.log('PGINSPECT '+path+' '+unreal.DataTableFunctionLibrary.export_data_table_to_json_string(a))
 elif a: unreal.log('PGINSPECT native_attack='+str(isinstance(unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path)),unreal.PGEnemyAbilityAttack)))



