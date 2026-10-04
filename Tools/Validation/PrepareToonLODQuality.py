"""Add the missing Inori dodge fixture without regenerating existing animations."""
import hashlib
import json
import os
from pathlib import Path
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = Path(os.environ['PG_TOON_QUALITY_OUT'])
report = {'status': 'RUNNING'}
source = '/Game/ExternalAssets/Animations/SwordAnimsetPro/Retargeted/AS_ElfSelenaDodge_F_Anim'
target = '/Game/Art/ToonTest/Inori/Animation/Quality/PGToon_AS_ElfSelenaDodge_F_Anim'

def digest(path):
    return hashlib.sha256((ROOT/'Content'/(path.removeprefix('/Game/')+'.uasset')).read_bytes()).hexdigest()

try:
    paths = [source, '/Game/Art/ToonTest/Inori/SK_Inori_ToonTest',
             '/Game/Art/ToonTest/Inori/Animation/RTG_PGToon_ElfSelena_Inori']
    before = {p: digest(p) for p in paths}
    clip = unreal.load_asset(target) if unreal.EditorAssetLibrary.does_asset_exist(target) else None
    if not clip:
        inputs = unreal.IKRetargetBatchOperationInputs()
        inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data(source)]
        inputs.source_mesh = unreal.load_asset('/Game/ExternalAssets/Characters/ElfSelena/BaseMesh/SK_ElfSelena')
        inputs.target_mesh = unreal.load_asset(paths[1])
        inputs.ik_retarget_asset = unreal.load_asset(paths[2])
        assert unreal.load_asset(source).get_editor_property('skeleton') == inputs.source_mesh.get_editor_property('skeleton')
        inputs.target_path = target.rsplit('/', 1)[0]
        inputs.prefix = 'PGToon_'
        inputs.include_referenced_assets = inputs.overwrite_existing_files = False
        assert len(unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)) == 1
        clip = unreal.load_asset(target)
        assert unreal.EditorAssetLibrary.save_loaded_asset(clip, False)
    assert clip and clip.get_play_length() > 0
    assert clip.get_editor_property('skeleton') == unreal.load_asset(paths[1]).get_editor_property('skeleton')
    assert before == {p: digest(p) for p in paths}
    mesh = unreal.load_asset('/Game/Art/ToonTest/Advanced/Performance/SK_Inori_ToonLOD')
    lods = mesh.get_editor_property('source_models')
    report.update(status='PASS', dodge={'label': 'Dodge', 'asset': target, 'duration': clip.get_play_length()},
                  preserved_sha256=before, lod_info=[{
                      'screen_size': x.get_editor_property('screen_size').get_editor_property('default'),
                      'hysteresis': x.get_editor_property('lod_hysteresis'),
                      'reduction': {k: str(x.get_editor_property('reduction_settings').get_editor_property(k))
                                    for k in ['num_of_triangles_percentage', 'num_of_vert_percentage',
                                              'base_lod', 'recalc_normals', 'remap_morph_targets']}} for x in lods])
except Exception:
    report.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(report['error'])
(OUT/'prepare.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
if report['status'] != 'PASS':
    raise RuntimeError(report['error'])
