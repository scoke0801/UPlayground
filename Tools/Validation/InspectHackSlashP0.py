"""Read-only audit of preserved dodge assets and effective input settings."""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()).resolve() / 'QA/HackSlashP0'
out.mkdir(parents=True, exist_ok=True)
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(
    unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')))
roll = next(r for r in rows if r['SkillID'] == 10000)
cls = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Actor/LocalPlayer/Ability/GA_Skill_Roll')
default = unreal.get_default_object(cls)
montage = unreal.load_asset(roll['MontagePath']['AssetPath']['PackageName'])
result = {'roll_row':roll, 'roll_distance':str(default.get_editor_property('rolling_distance_scalable_float')),
          'roll_montage_seconds':montage.get_play_length(), 'roll_rate_scale':montage.get_editor_property('rate_scale'),
          'roll_notifies':[n.export_text() for n in unreal.AnimationLibrary.get_animation_notify_events(montage)]}
curve = unreal.load_asset('/Game/Blueprints/Actor/LocalPlayer/GameplayEffect/CT_PlayerStats')
result['roll_distance_level_1'] = str(unreal.DataTableFunctionLibrary.evaluate_curve_table_row(curve, 'Player.RollingDistance', 1., 'HackSlashP0 audit'))
for asset in (default, montage):
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(out / (asset.get_name() + '.copy'))
    task.automated = True
    task.prompt = False
    task.exporter = unreal.ObjectExporterT3D()
    assert unreal.Exporter.run_asset_export_task(task)
player_cls = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Actor/LocalPlayer/BP_LocalPlayer')
player = unreal.get_default_object(player_cls)
asc = player.get_component_by_class(unreal.PGAbilitySystemComponent)
result['input_buffer_seconds'] = asc.get_editor_property('input_buffer_seconds')
(out/'preserved_settings.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('PGHackSlash AUDIT PASS')
