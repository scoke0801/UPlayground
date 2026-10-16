"""Back up and retune only the saved player's moving-attack blend; validate in a fresh process."""
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from PlayableCharacterTransaction import Transaction, write_json

PACKAGE = '/Game/Blueprints/Actor/LocalPlayer/Animation/ABP_LocalPlayer'
blueprint = unreal.load_asset(PACKAGE)
assert blueprint
base = blueprint.get_path_name()+':AnimGraph.'
layer = unreal.find_object(None, base+'AnimGraphNode_LayeredBoneBlend_0')
variable = unreal.find_object(None, base+'K2Node_VariableGet_0')
assert layer and variable
validate_only = '-PGCombatMotionValidate' in unreal.SystemLibrary.get_command_line()
latest = ROOT/'Saved/CombatMotion_LastBackup.txt'
def export_variable(destination):
    task = unreal.AssetExportTask()
    task.object = variable
    task.filename = str(destination)
    task.automated = True
    task.prompt = False
    task.exporter = unreal.ObjectExporterT3D()
    assert unreal.Exporter.run_asset_export_task(task)
    return destination.read_text(encoding='utf-8-sig')

if not validate_only:
    out = ROOT/'Saved/Backups/CombatMotion'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    transaction = Transaction(ROOT, out)
    transaction.prepare([PACKAGE], Path(__file__))
    latest.write_text(str(out), encoding='utf-8')
    write_json(out/'before.json', dict(layer=layer.get_editor_property('node').export_text(),
                                      variable=export_variable(out/'variable-before.copy')))
    assert unreal.PGHumanoidLocomotionTools.configure_player_attack_blend(blueprint)
    transaction.mark_written(PACKAGE)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    transaction.data['status'] = 'APPLIED_UNVERIFIED'
    transaction.flush()
node = layer.get_editor_property('node')
assert node.get_editor_property('mesh_space_rotation_blend')
out = Path(latest.read_text(encoding='utf-8'))
assert 'MemberName="bHasVelocity"' in export_variable(out/('variable-reloaded.copy' if validate_only else 'variable-after.copy'))
assert 'Spine_01' in node.export_text()
if validate_only:
    out = Path(latest.read_text(encoding='utf-8'))
    manifest = out/'transaction.json'
    transaction = json.loads(manifest.read_text(encoding='utf-8'))
    assert transaction['status'] in ('APPLIED_UNVERIFIED', 'VERIFIED')
    transaction['status'] = 'VERIFIED'
    write_json(manifest, transaction)
unreal.log('PGCombatMotion '+('VALIDATION' if validate_only else 'APPLY')+' PASS backup='+str(out))
