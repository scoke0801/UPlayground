"""Enable continuous primary attack; back up the action before changing its triggers."""
from datetime import datetime, timezone
from pathlib import Path
import shutil
import unreal

PATH = '/Game/Blueprints/Input/Actions/IA_Skill_Normal'
action = unreal.load_asset(PATH)
assert action, PATH
triggers = list(action.get_editor_property('triggers'))
assert all(isinstance(trigger, (unreal.InputTriggerPressed, unreal.InputTriggerReleased)) for trigger in triggers), 'Unexpected primary attack trigger; preserve custom behavior'
if triggers:
    relative = Path(PATH.removeprefix('/Game/') + '.uasset')
    backup = Path(unreal.Paths.project_saved_dir()) / 'Backups/CombatFeel' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') / relative
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(Path(unreal.Paths.project_content_dir()) / relative, backup)
    # No explicit triggers: Started on press, Triggered while down, Completed on release.
    action.set_editor_property('triggers', [])
    assert unreal.EditorAssetLibrary.save_loaded_asset(action), PATH
unreal.log('PGCombatFeel continuous primary attack configured')
exec(compile(Path(__file__).with_name('ValidateCombatFeel.py').read_text(encoding='utf-8'), 'ValidateCombatFeel.py', 'exec'))
