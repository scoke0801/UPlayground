import unreal, json, os
out = os.path.join(unreal.Paths.project_saved_dir(), 'RoguelikeMVP')
os.makedirs(out, exist_ok=True)
paths = {'enemies':'/Game/DataCenter/DataTables/Actor/DT_Enemy', 'skills':'/Game/DataCenter/DataTables/Skill/DT_Skill', 'stats':'/Game/DataCenter/DataTables/Actor/DT_CharacterStat', 'stages':'/Game/DataCenter/DataTables/Stage/DT_StageData'}
for key, path in paths.items():
    asset = unreal.load_asset(path)
    if asset:
        with open(os.path.join(out,key+'.json'),'w',encoding='utf-8') as f:
            f.write(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(asset))
    else: unreal.log_warning('Missing '+path)
with open(os.path.join(out,'tables.json'),'w',encoding='utf-8') as f:
    json.dump(list(unreal.EditorAssetLibrary.list_assets('/Game/DataCenter', recursive=True)),f)
