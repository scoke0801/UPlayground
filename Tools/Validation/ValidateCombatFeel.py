"""Read-only validation of the real primary-attack action and its mapping triggers."""
import unreal

def validate_combat_feel():
    action = unreal.load_asset('/Game/Blueprints/Input/Actions/IA_Skill_Normal')
    assert action and not action.get_editor_property('triggers'), 'Primary attack must trigger throughout a hold'
    found = 0
    for name in ['IMC_Default', 'IMC_Weapon']:
        context = unreal.load_asset('/Game/Blueprints/Input/' + name)
        assert context, name
        for mapping in context.get_editor_property('default_key_mappings').get_editor_property('mappings'):
            if mapping.get_editor_property('action') == action:
                found += 1
                assert not mapping.get_editor_property('triggers'), f'{name}: mapping overrides continuous attack'
    assert found, 'Primary attack has no input mapping'
    roll = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Actor/LocalPlayer/Ability/GA_Skill_Roll'))
    assert isinstance(roll, unreal.PGAbilitySkill_Roll), 'Dodge must use the native direction/warp implementation'
    unreal.log(f'PGCombatFeel VALIDATION PASS mappings={found}')
    return dict(continuous_attack=True, attack_mappings=found, native_dodge=True)

if __name__ == '__main__':
    validate_combat_feel()
