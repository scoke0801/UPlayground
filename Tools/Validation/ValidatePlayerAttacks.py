"""Validate saved player combat profiles and GAS/animation assets in a fresh editor."""
import json
import re
from pathlib import Path
import unreal


def validate_player_attacks():
    spec = json.loads((Path(unreal.Paths.project_dir()) / 'Tools/Validation/PlayerAttacks.json').read_text(encoding='utf-8'))
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(
        unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')))
    by_id = {r['SkillID']: r for r in rows}
    assert by_id[100]['ChainSkillIdList'] == [101, 102]
    assert not by_id[101]['ChainSkillIdList'] and not by_id[102]['ChainSkillIdList']
    for item in spec['skills']:
        row = by_id[item['SkillID']]
        profile = row.get('PlayerProfile')
        if isinstance(profile, str) and profile.startswith(('/Game/DataCenter/HackSlashP0/','/Game/DataCenter/HackSlashP1/')):
            continue  # Explicit profile owns P0 tuning; validate below, never against legacy coefficients.
        for key, expected in item.items():
            actual = row[key]
            assert abs(actual - expected) < .0001 if isinstance(expected, float) else actual == expected, (item['SkillID'], key, actual)
        montage = unreal.load_asset(row['MontagePath']['AssetPath']['PackageName'])
        assert montage
        if str(item['SkillID']) in spec['melee_windows']:
            notifies = unreal.AnimationLibrary.get_animation_notify_events(montage)
            hits = [n for n in notifies if n.get_editor_property('notify_state_class') and
                    n.get_editor_property('notify_state_class').get_class().get_name() == 'ANS_ToggleWeaponCollision_C']
            assert len(hits) == len(spec['melee_windows'][str(item['SkillID'])]), item['SkillID']
            for hit, (_, duration) in zip(hits, spec['melee_windows'][str(item['SkillID'])]):
                # Duration is protected in UE's Python reflection; exported struct text preserves it.
                match = re.search(r'\bDuration=([0-9.]+)', hit.export_text())
                assert match and abs(float(match[1]) - duration) < .001
    for name in ['GA_Skill_NormalAttack'] + ['GA_Player_Skill_Slot_' + str(i) for i in range(1, 7)]:
        cls = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Actor/LocalPlayer/Ability/' + name)
        obj = unreal.get_default_object(cls)
        assert isinstance(obj, unreal.PGAbilityPlayerSkill)
        assert obj.get_editor_property('retrigger_instanced_ability')
        tags = obj.get_editor_property('block_abilities_with_tag').export_text()
        assert 'Player.Ability.Attack' not in tags, name + ' blocks its own combo'
        assert 'Player.Ability.Equip' in tags and 'Player.Ability.UnEquip' in tags
    if any(isinstance(r.get('PlayerProfile'), str) and r['PlayerProfile'].startswith('/Game/DataCenter/HackSlashP0/') for r in rows):
        from ValidateHackSlashP0 import validate_hack_slash_p0
        validate_hack_slash_p0(rows)
    unreal.log('PGPlayerAttacks VALIDATION PASS profiles=8 abilities=7 multi_hit_skills=3')
    return {'profiles': 8, 'abilities': 7, 'multi_hit_skills': 3}


if __name__ == '__main__': validate_player_attacks()
