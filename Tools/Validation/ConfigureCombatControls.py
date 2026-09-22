"""Remove obsolete player equip/unequip input bindings, preserving other inputs."""
import os, shutil, unreal

backup = os.path.join(unreal.Paths.project_saved_dir(), 'Backups', 'CombatControls')

def preserve(path):
    relative = path.removeprefix('/Game/') + '.uasset'
    source = os.path.join(unreal.Paths.project_content_dir(), relative)
    dest = os.path.join(backup, relative)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    if os.path.exists(source) and not os.path.exists(dest): shutil.copy2(source, dest)

actions = [unreal.load_asset('/Game/Blueprints/Input/Actions/'+name) for name in ['IA_EquipWeapon','IA_UnEquipWeapon']]
path = '/Game/Blueprints/Actor/LocalPlayer/DA_InputConfig'
config = unreal.load_asset(path)
bindings = list(config.get_editor_property('ability_input_actions'))
filtered = [entry for entry in bindings if entry.get_editor_property('input_action') not in actions]
if len(filtered) != len(bindings):
    preserve(path)
    config.set_editor_property('ability_input_actions', filtered)
    assert unreal.EditorAssetLibrary.save_loaded_asset(config)

for name in ['IMC_Default','IMC_Weapon']:
    path = '/Game/Blueprints/Input/' + name
    context = unreal.load_asset(path)
    preserve(path)
    for action in actions:
        if action: context.unmap_all_keys_from_action(action)
    assert unreal.EditorAssetLibrary.save_loaded_asset(context)

path = '/Game/Blueprints/Actor/LocalPlayer/Weapon/BP_PlayerWeapon_Sword'
weapon = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
data = weapon.get_editor_property('weapon_data')
abilities = list(data.get_editor_property('default_weapon_abilities'))
filtered = [entry for entry in abilities if 'UnEquip' not in entry.get_editor_property('ability_to_grant').get_name()]
if len(filtered) != len(abilities):
    preserve(path)
    data.set_editor_property('default_weapon_abilities', filtered)
    weapon.set_editor_property('weapon_data', data)
    assert unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
unreal.log('PGCombatControls obsolete input bindings removed')
