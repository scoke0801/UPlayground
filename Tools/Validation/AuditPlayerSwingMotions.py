"""Read-only audit of player attack source contacts, notifies and pose mappings."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')))
registry=unreal.AssetRegistryHelpers.get_asset_registry()
result={'skills': []}
for row in rows:
    if row['SkillID'] not in (100,101,102,110,111,112,113,114): continue
    montage=unreal.load_asset(row['MontagePath']['AssetPath']['PackageName'])
    profile=unreal.load_asset(row['PlayerProfile'])
    record={'id': row['SkillID'], 'montage': montage.get_path_name(), 'length': montage.get_play_length(),
            'pose_keys':[p.export_text() for p in profile.get_editor_property('pose_keys')],
            'hit_phases':[p.export_text() for p in profile.get_editor_property('hit_phases')], 'motions':[]}
    for asset in [montage]+[unreal.load_asset(str(p)) for p in registry.get_dependencies(montage.get_path_name().split('.')[0], unreal.AssetRegistryDependencyOptions(include_hard_package_references=True)) if str(p).startswith('/Game/')]:
        if not isinstance(asset, unreal.AnimSequenceBase): continue
        record['motions'].append({'path':asset.get_path_name(),'length':asset.get_play_length(),
                                 'notifies':[e.export_text() for e in unreal.AnimationLibrary.get_animation_notify_events(asset)]})
    result['skills'].append(record)
(root/'Saved/QA/PlayerSwingMotionAudit.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
unreal.log('PGSwingMotion AUDIT PASS')
