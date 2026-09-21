import unreal
path='/Game/DataCenter/DataTables/Actor/DT_CharacterStat'
unreal.log(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))
