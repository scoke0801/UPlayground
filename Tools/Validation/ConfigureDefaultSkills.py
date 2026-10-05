"""Apply/verify the four-slot default without changing combat tuning or saved profiles.

Pass -PGApplyDefaultSkills to back up and update only DA_PGProgression.
"""
from datetime import datetime, timezone
from pathlib import Path
import json
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
catalog = unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
defaults = [111, 112, 110, 114]
assert catalog and set(defaults) <= set(catalog.get_editor_property('selectable_active_skills'))
if '-PGApplyDefaultSkills' in unreal.SystemLibrary.get_command_line():
    relative = Path('DataCenter/Progression/DA_PGProgression.uasset')
    backup = root / 'Saved/Backups/DefaultSkills' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup.mkdir(parents=True)
    shutil.copy2(root / 'Content' / relative, backup / relative.name)
    catalog.set_editor_property('default_active_skills', defaults)
    assert unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False)
    unreal.log('PGDefaultSkills backup=' + str(backup))
assert list(catalog.get_editor_property('default_active_skills')) == defaults

config = unreal.load_asset('/Game/Blueprints/Actor/LocalPlayer/DA_InputConfig')
bindings = config.get_editor_property('ability_input_actions')
weapon_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Actor/LocalPlayer/Weapon/BP_PlayerWeapon_Sword')
weapon = unreal.get_default_object(weapon_class).get_editor_property('weapon_data')
grants = weapon.get_editor_property('default_weapon_abilities')
for index in range(1, 5):
    tag = 'InputTag.Skill_Slot' + str(index)
    binding = next(b for b in bindings if str(b.get_editor_property('input_tag').get_editor_property('tag_name')) == tag)
    action = binding.get_editor_property('input_action')
    keys = []
    for name in ('IMC_Default', 'IMC_Weapon'):
        context = unreal.load_asset('/Game/Blueprints/Input/' + name)
        for mapping in context.get_editor_property('default_key_mappings').get_editor_property('mappings'):
            if mapping.get_editor_property('action') == action:
                keys.append(str(mapping.get_editor_property('key').export_text()))
    assert keys, (index, 'missing input mapping')
    ability_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Actor/LocalPlayer/Ability/GA_Player_Skill_Slot_' + str(index))
    ability = unreal.get_default_object(ability_class)
    assert ability.get_editor_property('slot_index') == getattr(unreal.PGSkillSlot, 'SKILL_SLOT_' + str(index))
    assert any(g.get_editor_property('ability_to_grant') == ability_class for g in grants), (index, 'missing ability grant')
    unreal.log('PGDefaultSkills slot=' + str(index) + ' skill=' + str(defaults[index-1]) + ' keys=' + json.dumps(keys))
unreal.log('PGDefaultSkills VALIDATION PASS slots=4')
