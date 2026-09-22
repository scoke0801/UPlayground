"""Import ImageGen skill icons and change only SkillIconPath. UE 5.8 commandlet."""
import unreal, os, json, shutil, datetime
root=unreal.Paths.project_dir()
destination='/Game/UI/SkillIcons'
mapping={100:'Normal',101:'Normal',102:'Normal',110:'Jump',111:'Dash',112:'Spin',113:'Chain',114:'Aura',115:'Ultimate',10000:'Roll'}
table_path='/Game/DataCenter/DataTables/Skill/DT_Skill'
table=unreal.load_asset(table_path)
assert table
before=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
assert set(mapping).issubset({r['SkillID'] for r in before})
for name in set(mapping.values()):
 assert os.path.isfile(os.path.join(root,'Tools/Art/SkillIcons/T_Skill_'+name+'.png')),name
backup=os.path.join(root,'Saved/Backups/SkillIcons',datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
os.makedirs(backup,exist_ok=True)
shutil.copy2(os.path.join(root,'Content/DataCenter/DataTables/Skill/DT_Skill.uasset'),backup)
with open(os.path.join(backup,'DT_Skill.json'),'w',encoding='utf-8') as f:json.dump(before,f,ensure_ascii=False,indent=2)
for name in sorted(set(mapping.values())):
 task=unreal.AssetImportTask()
 task.filename=os.path.join(root,'Tools/Art/SkillIcons/T_Skill_'+name+'.png')
 task.destination_path=destination
 task.destination_name='T_Skill_'+name
 task.automated=True;task.replace_existing=True;task.save=False
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 texture=unreal.load_asset(destination+'/'+task.destination_name)
 assert isinstance(texture,unreal.Texture2D),name
 texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
 texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
 texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
 texture.set_editor_property('max_texture_size',256)
 texture.set_editor_property('srgb',True)
 assert unreal.EditorAssetLibrary.save_loaded_asset(texture,only_if_is_dirty=False),name
updated=json.loads(json.dumps(before))
for row in updated:
 if row['SkillID'] in mapping:
  name='T_Skill_'+mapping[row['SkillID']]
  row['SkillIconPath']={'AssetPath':{'PackageName':destination+'/'+name,'AssetName':name},'SubPathString':''}
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(updated,ensure_ascii=False))
after=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
assert after==updated,'Unexpected data-table roundtrip change; table not saved'
assert unreal.EditorAssetLibrary.save_loaded_asset(table,only_if_is_dirty=False)
for row in after:
 if row['SkillID'] in mapping:
  assert unreal.load_asset(row['SkillIconPath']['AssetPath']['PackageName'])
report={'icons':len(set(mapping.values())),'updated_rows':len(mapping),'backup':backup,'unrelated_fields_preserved':True,'mapping':mapping}
with open(os.path.join(root,'Saved/SkillIconImport.json'),'w',encoding='utf-8') as f:json.dump(report,f,ensure_ascii=False,indent=2)
unreal.log('PG SkillIcons: imported 8 icons; updated 10 player rows; all other fields preserved')