import unreal, json, os
registry=unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')))
result={'skills':[], 'abilities':{}, 'loadouts':[]}
for row in rows:
 if not row['Name'].startswith('Player'): continue
 record=dict(row)
 path=row['MontagePath']['AssetPath']['PackageName']
 if path!='None':
  montage=unreal.load_asset(path)
  record['length']=montage.get_play_length()
  record['montage_properties']={}
  for prop in ['slot_anim_tracks','notifies','composite_sections','skeleton']:
   try: record['montage_properties'][prop]=str(montage.get_editor_property(prop))
   except Exception as e: record['montage_properties'][prop]=str(e)
  record['dependencies']=[str(x) for x in registry.get_dependencies(path, unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True))]
 result['skills'].append(record)
for item in registry.get_assets_by_path('/Game/Blueprints/Actor/LocalPlayer',recursive=True):
 if 'Ability' in str(item.package_name):
  result['abilities'][str(item.package_name)]=[str(x) for x in registry.get_dependencies(item.package_name,unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True))]
data=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
for build in data.get_editor_property('builds'):
 result['loadouts'].append({'id':str(build.get_editor_property('id')),'skills':[{'slot':str(x.get_editor_property('slot')),'id':x.get_editor_property('skill_id')} for x in build.get_editor_property('skills')]})
with open(os.path.join(unreal.Paths.project_saved_dir(),'SkillMotionAudit.json'),'w',encoding='utf-8') as f:json.dump(result,f,ensure_ascii=False,indent=2)