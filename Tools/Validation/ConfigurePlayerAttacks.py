"""Back up and migrate player attack data, GAS self-blocking and multi-hit windows."""
from datetime import datetime, timezone
import json
import re
from pathlib import Path
import shutil
import unreal

ROOT = Path(unreal.Paths.project_dir())
SPEC = json.loads((ROOT / 'Tools/Validation/PlayerAttacks.json').read_text(encoding='utf-8'))
BACKUP = ROOT / 'Saved/Backups/PlayerAttacks' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')


def preserve(asset):
    relative = asset.get_path_name().split('.')[0].removeprefix('/Game/') + '.uasset'
    target = BACKUP / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists(): shutil.copy2(ROOT / 'Content' / relative, target)


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


# Read and validate all targets before writing anything.
table = unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
by_id = {r['SkillID']: r for r in rows}
montages = {}
for item in SPEC['skills']:
    row = by_id[item['SkillID']]
    for field in item: assert field in row, 'Build the updated native row before applying: ' + field
    montage = unreal.load_asset(row['MontagePath']['AssetPath']['PackageName'])
    assert montage
    montages[item['SkillID']] = montage
abilities = []
for name in ['GA_Skill_NormalAttack'] + ['GA_Player_Skill_Slot_' + str(i) for i in range(1, 7)]:
    bp = unreal.load_asset('/Game/Blueprints/Actor/LocalPlayer/Ability/' + name)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(bp.get_path_name())
    assert cls and isinstance(unreal.get_default_object(cls), unreal.PGAbilityPlayerSkill)
    abilities.append((bp, unreal.get_default_object(cls)))
notify_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Shared/AnimationNotifyState/ANS_ToggleWeaponCollision')
assert notify_class
for asset in [table] + [bp for bp, _ in abilities] + list(montages.values()): preserve(asset)
(BACKUP / 'skills.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf-8')

for bp, obj in abilities:
    tags = obj.get_editor_property('block_abilities_with_tag')
    # Leave equipment exclusions intact. Native CanStartSkill owns the attack/dodge cancel gate.
    names = re.findall(r'TagName="([^"]+)"', tags.export_text())
    names = [name for name in names if name != 'Player.Ability.Attack']
    tags.import_text('(GameplayTags=(' + ','.join('(TagName="' + name + '")' for name in names) + '))')
    obj.set_editor_property('block_abilities_with_tag', tags)
    obj.set_editor_property('retrigger_instanced_ability', True)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)

for item in SPEC['skills']: by_id[item['SkillID']].update(item)
by_id[100]['ChainSkillIdList'] = [101, 102]
by_id[101]['ChainSkillIdList'] = []
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False))
save(table)

lib = unreal.AnimationLibrary
for key, windows in SPEC['melee_windows'].items():
    montage = montages[int(key)]
    for event in lib.get_animation_notify_events(montage):
        state = event.get_editor_property('notify_state_class')
        if state and state.get_class() == notify_class:
            lib.remove_animation_notify_events_by_name(montage, event.get_editor_property('notify_name'))
    track = lib.get_animation_notify_track_names(montage)[0]
    for start, duration in windows:
        assert start + duration < montage.get_play_length()
        assert lib.add_animation_notify_state_event(montage, track, start, duration, notify_class)
    save(montage)
unreal.log('PGPlayerAttacks APPLY PASS backup=' + str(BACKUP))
