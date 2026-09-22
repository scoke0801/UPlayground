"""Read-only asset audit for AI movement, cursor aim and startup weapon."""
import unreal, json, os

out = os.path.join(unreal.Paths.project_saved_dir(), 'CombatControls')
os.makedirs(out, exist_ok=True)
result = {}

def properties(obj, names):
    data = {'object': obj.get_path_name() if obj else None}
    if obj:
        for name in names:
            try: data[name] = str(obj.get_editor_property(name))
            except Exception as error: data[name] = str(error)
    return data

for name, path in [('skills','/Game/DataCenter/DataTables/Skill/DT_Skill'), ('enemies','/Game/DataCenter/DataTables/Actor/DT_Enemy')]:
    result[name] = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))
paths = ['/Game/Blueprints/Actor/LocalPlayer/BP_LocalPlayer',
         '/Game/Blueprints/Actor/LocalPlayer/Ability/GA_PlayerEquipSword',
         '/Game/Blueprints/Shared/Ability/GA_SpawnWeapon',
         '/Game/Blueprints/Shared/Ability/GA_UnEquipWeapon',
         '/Game/Blueprints/Actor/LocalPlayer/Weapon/BP_PlayerWeapon_Sword']
for path in paths + ['/Game/Blueprints/Actor/LocalPlayer/DA_PlayerConfig']:
    obj = unreal.load_asset(path)
    task = unreal.AssetExportTask()
    task.object = obj
    task.filename = os.path.join(out, obj.get_name()+'.copy')
    task.automated = True
    task.prompt = False
    task.exporter = unreal.ObjectExporterT3D()
    unreal.Exporter.run_asset_export_task(task)
result['defaults'] = []
for path in paths:
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    obj = unreal.get_default_object(cls) if cls else None
    result['defaults'].append(properties(obj, ['character_start_up_data','weapon_class_to_spawn','socket_name_to_attach_to','register_as_equipped_weapon','weapon_tag_to_register','weapon_tag','attach_socket_name','ability_activation_policy','weapon_data']))
    task = unreal.AssetExportTask()
    task.object = obj
    task.filename = os.path.join(out, obj.get_name()+'.copy')
    task.automated = True
    task.prompt = False
    task.exporter = unreal.ObjectExporterT3D()
    unreal.Exporter.run_asset_export_task(task)
    if path.endswith('BP_LocalPlayer'):
        result['player_startup'] = properties(unreal.load_asset('/Game/Blueprints/Actor/LocalPlayer/DA_PlayerConfig'), ['activate_on_given_abilities','reactive_abilities','player_start_up_ability_sets'])
        result['player_input'] = properties(obj.get_editor_property('input_config_data_asset'), ['ability_input_actions'])
        result['sockets'] = [str(x) for x in obj.get_editor_property('mesh').get_all_socket_names()]

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level.load_level('/Game/Maps/RogueArena')
result['navigation'] = []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if 'Nav' in actor.get_class().get_name():
        result['navigation'].append(properties(actor, ['runtime_generation','agent_radius','agent_height']) | {'location':str(actor.get_actor_location()),'bounds':str(actor.get_actor_bounds(False))})
        task = unreal.AssetExportTask()
        task.object = actor
        task.filename = os.path.join(out, actor.get_class().get_name()+'.copy')
        task.automated = True
        task.prompt = False
        task.exporter = unreal.ObjectExporterT3D()
        unreal.Exporter.run_asset_export_task(task)

result['trees'] = []
for path in unreal.EditorAssetLibrary.list_assets('/Game/Blueprints/Actor/NonPlayer/Enemy', recursive=True):
    if '/BT_' not in path: continue
    tree = unreal.load_asset(path)
    result['trees'].append(properties(tree,['root_node','blackboard_asset']))
    task = unreal.AssetExportTask()
    task.object = tree
    task.filename = os.path.join(out, tree.get_name()+'.copy')
    task.automated = True
    task.prompt = False
    task.exporter = unreal.ObjectExporterT3D()
    unreal.Exporter.run_asset_export_task(task)
with open(os.path.join(out, 'Assets.json'), 'w', encoding='utf-8') as stream:
    json.dump(result, stream, ensure_ascii=False, indent=2)
unreal.log('PGCombatControls asset audit complete')
