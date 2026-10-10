"""UE metadata inspection plus an Art animation use-candidate inventory."""
import collections
import csv
import json
import os
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = Path(os.environ['PG_DARK_KNIGHT_RUN'])
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
result = dict(meshes={}, motions=[], tables={})
paths = [
    '/Game/ExternalAssets/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full_With_Sword',
    '/Game/ExternalAssets/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full',
    '/Game/ExternalAssets/Animations/BossyEnemy/SkeletalMesh/SK_Mannequin_UE4_WithWeapon']
for path in paths:
    mesh = unreal.load_asset(path); assert isinstance(mesh, unreal.SkeletalMesh), path
    comp = unreal.SkeletalMeshComponent(); comp.set_skeletal_mesh_asset(mesh)
    result['meshes'][path] = dict(skeleton=mesh.get_editor_property('skeleton').get_path_name(),
        bounds=str(mesh.get_bounds()), bones=[str(comp.get_bone_name(i)) for i in range(comp.get_num_bones())],
        materials=[str(m.material_interface.get_path_name()) for m in mesh.get_editor_property('materials')])
for asset in registry.get_assets_by_path('/Game/ExternalAssets/Animations/BossyEnemy/Animations', recursive=True):
    if str(asset.asset_class_path.asset_name) != 'AnimSequence': continue
    clip = asset.get_asset()
    result['motions'].append(dict(asset=str(asset.package_name), length=clip.get_play_length(),
        skeleton=clip.get_editor_property('skeleton').get_path_name(), tracks=[str(n) for n in unreal.AnimationLibrary.get_animation_track_names(clip)]))
for name,path in [('enemies','Actor/DT_Enemy'),('skills','Skill/DT_Skill'),('stats','Actor/DT_CharacterStat'),('deaths','Path/DT_Death'),('stages','Stage/DT_StageData')]:
    result['tables'][name] = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/DataCenter/DataTables/'+path)))
(OUT/'inspection.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')

# Class-based scan includes Animation, Animations and other folders without filename assumptions.
rows = []
for asset in registry.get_assets_by_path('/Game/Art', recursive=True):
    cls = str(asset.asset_class_path.asset_name)
    if cls not in ('AnimSequence','AnimMontage','BlendSpace','BlendSpace1D','AnimBlueprint','AnimComposite','PoseAsset'): continue
    package = str(asset.package_name)
    refs = sorted(str(n) for n in registry.get_referencers(asset.package_name, options))
    outside = [p for p in refs if not p.startswith('/Game/Art/')]
    rows.append(dict(asset=package, asset_class=cls, direct_referencers=len(refs),
        status='referenced_outside_art' if outside else 'referenced_within_art' if refs else 'no_registry_referencer_candidate',
        outside_art_referencers=';'.join(outside), referencers=';'.join(refs)))
dest = ROOT/'Docs/todo/assets/MotionExpansion'
dest.mkdir(parents=True, exist_ok=True)
assert rows, 'Art animation inventory was empty'
with (dest/'art-animation-inventory.csv').open('w', encoding='utf-8-sig', newline='') as stream:
    writer=csv.DictWriter(stream, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(sorted(rows,key=lambda r:r['asset']))
summary = dict(total=len(rows), classes=dict(collections.Counter(r['asset_class'] for r in rows)),
    status=dict(collections.Counter(r['status'] for r in rows)),
    scope='/Game/Art recursively, actual UE animation asset classes',
    limitation='Registry references are not proof of active gameplay use; string loads and unreachable galleries need separate review.')
(dest/'art-animation-summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('PGDarkKnight inspect PASS '+json.dumps(summary))
