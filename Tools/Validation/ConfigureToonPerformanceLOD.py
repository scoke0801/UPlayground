"""Build an opt-in Inori LOD candidate; preserve the source mesh, maps and skeleton."""
import hashlib
import json
import shutil
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT/'Saved/ToonTest/PerformanceLOD'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
REPORT = {'status': 'RUNNING', 'run': str(OUT)}
SOURCE = '/Game/Art/ToonTest/Inori/SK_Inori_ToonTest'
DEST = '/Game/Art/ToonTest/Advanced/Performance/SK_Inori_ToonLOD'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


try:
    source_file = ROOT/'Content/Art/ToonTest/Inori/SK_Inori_ToonTest.uasset'
    before = digest(source_file)
    source = unreal.load_asset(SOURCE)
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    destination_file = ROOT/'Content/Art/ToonTest/Advanced/Performance/SK_Inori_ToonLOD.uasset'
    if destination_file.exists():
        shutil.copy2(destination_file, OUT/'previous_mesh.uasset')
        # Reuse candidate; regeneration never touches the source asset.
        mesh = unreal.load_asset(DEST)
    else:
        mesh = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, DEST)
    assert mesh and mesh.get_editor_property('skeleton') == source.get_editor_property('skeleton')
    assert subsystem.regenerate_lod(mesh, 3, False, False)
    row = next(r for r in json.loads((ROOT/'Saved/ToonTest/lighting_lab.json').read_text())['characters'] if r['name'] == 'Inori')
    no_shadow = [i for i, mat in enumerate(row['materials']) if mat['overlay'] or mat['profile'] in ['face', 'detail']]
    lods = []
    for lod in range(subsystem.get_lod_count(mesh)):
        sections = []
        for section in range(subsystem.get_num_sections(mesh, lod)):
            slot = subsystem.get_lod_material_slot(mesh, lod, section)
            if slot in no_shadow and subsystem.get_section_cast_shadow(mesh, lod, section):
                assert subsystem.set_section_cast_shadow(mesh, lod, section, False)
            if slot in no_shadow:
                assert subsystem.get_section_cast_shadow(mesh, lod, section) is False
            sections.append(dict(section=section, slot=slot, casts_shadow=subsystem.get_section_cast_shadow(mesh, lod, section)))
        lods.append(dict(lod=lod, vertices=subsystem.get_num_verts(mesh, lod), sections=sections))
    assert lods[0]['vertices'] == subsystem.get_num_verts(source, 0)
    assert 0 < lods[2]['vertices'] < lods[1]['vertices'] < lods[0]['vertices'], lods
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    assert digest(source_file) == before
    # A separate six-motion map makes the measured LOD1 candidate reviewable in PIE.
    # Original lighting and motion maps remain the regression baseline.
    map_path = '/Game/Art/ToonTest/Maps/L_PGToon_PerformanceMotionTest'
    map_file = ROOT/'Content/Art/ToonTest/Maps/L_PGToon_PerformanceMotionTest.umap'
    if map_file.exists():
        shutil.copy2(map_file, OUT/'previous_map.umap')
    world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Art/ToonTest/Maps/L_PGToon_AdvancedMotionTest')
    assert world
    preview_actors = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                      if isinstance(a, unreal.PGToonPreviewActor)]
    assert len(preview_actors) == 6
    for actor in preview_actors:
        component = actor.skeletal_mesh_component
        animation = component.get_editor_property('animation_data')
        overrides = [component.get_material(i) for i in range(component.get_num_materials())]
        component.set_skeletal_mesh_asset(mesh)
        for slot, material in enumerate(overrides):
            component.set_material(slot, material)
        component.set_editor_property('animation_data', animation)
        component.set_forced_lod(2)  # UE uses 1-based forced LOD values.
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)
    REPORT.update(status='PASS', source=SOURCE, mesh=DEST, source_sha256=before, source_unchanged=True,
                  lods=lods, disabled_shadow_slots=no_shadow, map=map_path, map_forced_lod=1,
                  limits=['Opt-in test candidate; original mesh and gameplay assets unchanged',
                          'LOD thresholds and continuous motion quality require stage 2 review'])
except Exception:
    REPORT.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    payload = json.dumps(REPORT, indent=2)
    (OUT/'configure.json').write_text(payload, encoding='utf-8')
    (ROOT/'Saved/ToonTest/performance_lod.json').write_text(payload, encoding='utf-8')
if REPORT['status'] != 'PASS':
    raise RuntimeError('Toon performance LOD configuration failed')
