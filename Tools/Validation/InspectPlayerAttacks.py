"""Read-only audit of the actual player attack defaults and montage notify times."""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'PlayerAttacks'
out.mkdir(parents=True, exist_ok=True)
table = unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
result = {'skills': [r for r in rows if 100 <= r['SkillID'] <= 115], 'abilities': {}}
for name in ['GA_Skill_NormalAttack'] + ['GA_Player_Skill_Slot_' + str(i) for i in range(1, 7)]:
    cls = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Actor/LocalPlayer/Ability/' + name)
    obj = unreal.get_default_object(cls)
    props = {}
    for prop in ['instancing_policy', 'retrigger_instanced_ability', 'slot_index', 'activation_owned_tags', 'activation_blocked_tags', 'block_abilities_with_tag', 'cancel_abilities_with_tag']:
        try: props[prop] = str(obj.get_editor_property(prop))
        except Exception as exc: props[prop] = str(exc)
    result['abilities'][name] = props
    task = unreal.AssetExportTask()
    task.object = obj
    task.filename = str(out / (name + '.copy'))
    task.automated = True
    task.prompt = False
    task.exporter = unreal.ObjectExporterT3D()
    unreal.Exporter.run_asset_export_task(task)
for row in result['skills']:
    path = row['MontagePath']['AssetPath']['PackageName']
    if path == 'None': continue
    montage = unreal.load_asset(path)
    task = unreal.AssetExportTask()
    task.object = montage
    task.filename = str(out / (montage.get_name() + '.copy'))
    task.automated = True
    task.prompt = False
    task.exporter = unreal.ObjectExporterT3D()
    unreal.Exporter.run_asset_export_task(task)
(out / 'audit.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('PGPlayerAttacks AUDIT PASS')
