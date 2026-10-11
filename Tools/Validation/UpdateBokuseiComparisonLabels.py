"""Refresh only comparison labels and audit saved mesh shading; use -PGLabelsVerify to read back."""
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
MAP = '/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_ShadingComparison'
LABELS = '/Game/Art/ToonTest/BokuseiShadingComparison/Labels'
RUN = ROOT/'Saved/BokuseiShadingComparison/Labels'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
RUN.mkdir(parents=True)
VERIFY = '-PGLabelsVerify' in unreal.SystemLibrary.get_command_line()
EAL = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary


def backup(path):
    source = ROOT/'Content'/(path.removeprefix('/Game/') + ('.umap' if path == MAP else '.uasset'))
    target = RUN/'backup'/source.relative_to(ROOT/'Content')
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


if not VERIFY:
    backup(MAP)
    for name in ['Title', 'Toon', 'FaceSDF']:
        path = LABELS+'/T_PGComparison_'+name
        backup(path)
        task = unreal.AssetImportTask()
        task.filename = str(ROOT/'Tools/Art/ToonTest/BokuseiShadingComparison'/(name+'.png'))
        task.destination_path, task.destination_name = LABELS, 'T_PGComparison_'+name
        task.automated, task.replace_existing, task.save = True, True, False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(path)
        assert texture
        texture.set_editor_property('srgb', True)
        texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property('never_stream', True)
        assert EAL.save_loaded_asset(texture, only_if_is_dirty=False)

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
renames = {'안내 · bOKUSEI 셰이딩 비교': '안내 · 단계별 셰이딩 비교',
           '5 · 외곽선 · 기존 툰': '5 · 외곽선',
           '안내 · 5 · 외곽선 · 기존 툰': '안내 · 5 · 외곽선'}
rows = []
for actor in actors:
    label = actor.get_actor_label()
    if label in renames:
        assert not VERIFY, ('Old label persisted', label)
        actor.set_actor_label(renames[label])
    if not isinstance(actor, unreal.SkeletalMeshActor):
        continue
    component = actor.skeletal_mesh_component
    mesh = component.get_editor_property('skeletal_mesh_asset')
    if not mesh:
        continue
    sdf = []
    for index in range(component.get_num_materials()):
        material = component.get_material(index)
        if isinstance(material, unreal.MaterialInstanceConstant):
            enabled = LIB.get_material_instance_scalar_parameter_value(material, 'FaceSDFEnabled')
            # Imported overrides can outlive a parent change (e.g. the Lit stage).
            # Count SDF only when the active master actually exposes its texture.
            master = material
            while isinstance(master, unreal.MaterialInstanceConstant):
                master = master.get_editor_property('parent')
            parameters = [str(p) for p in LIB.get_texture_parameter_names(master)]
            if enabled > .5 and 'FaceSDFTexture' in parameters:
                sdf.append(dict(slot=index, material=material.get_path_name()))
    proxy = actor.hair_shadow_proxy if isinstance(actor, unreal.PGToonPreviewActor) else None
    hair = bool(proxy and proxy.get_editor_property('skeletal_mesh_asset') and proxy.get_editor_property('cast_shadow'))
    rows.append(dict(actor=actor.get_actor_label(), class_name=actor.get_class().get_name(),
                     tags=[str(t) for t in actor.tags], sdf=sdf, hair_shadow=hair,
                     presentation=isinstance(actor, unreal.PGToonPreviewActor)))
if not VERIFY:
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
assert len(rows) >= 32, len(rows)
for row in rows:
    tags = row['tags']
    stage = next((int(t.removeprefix('PGShadingStage')) for t in tags if t.startswith('PGShadingStage')), None)
    if stage is None:
        stage = next((int(t.rsplit('_', 1)[1]) for t in tags if t.startswith('PGShadingGuestStage_')), None)
    if stage is not None:
        assert bool(row['sdf']) == (stage in [6, 7]), row
        assert row['hair_shadow'] == (stage in [5, 7]), row
report = dict(status='PASS', verify=VERIFY, actors=rows, run=str(RUN))
(RUN/'audit.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('PG label update/audit PASS: '+str(RUN))
