"""Fresh-process candidate persistence and original lighting/gallery regression."""
import hashlib
import json
import runpy
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
REPORT = {'status': 'RUNNING'}
try:
    data = json.loads((ROOT/'Saved/ToonTest/performance_lod.json').read_text())
    assert data['status'] == 'PASS'
    source = unreal.load_asset(data['source'])
    candidate = unreal.load_asset(data['mesh'])
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    assert source and candidate and source != candidate
    assert source.get_editor_property('skeleton') == candidate.get_editor_property('skeleton')
    assert source.get_editor_property('physics_asset') == candidate.get_editor_property('physics_asset')
    source_slots, candidate_slots = source.get_editor_property('materials'), candidate.get_editor_property('materials')
    assert len(source_slots) == len(candidate_slots)
    assert all(a.material_slot_name == b.material_slot_name and a.material_interface == b.material_interface
               for a, b in zip(source_slots, candidate_slots))
    assert hashlib.sha256((ROOT/'Content/Art/ToonTest/Inori/SK_Inori_ToonTest.uasset').read_bytes()).hexdigest() == data['source_sha256']
    assert subsystem.get_lod_count(candidate) == len(data['lods']) == 3
    for lod in data['lods']:
        index = lod['lod']
        assert subsystem.get_num_verts(candidate, index) == lod['vertices']
        assert subsystem.get_num_sections(candidate, index) == len(lod['sections'])
        for section in lod['sections']:
            assert subsystem.get_lod_material_slot(candidate, index, section['section']) == section['slot']
            assert subsystem.get_section_cast_shadow(candidate, index, section['section']) == section['casts_shadow']
    world = unreal.EditorLoadingAndSavingUtils.load_map(data['map'])
    assert world
    previews = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                if isinstance(a, unreal.PGToonPreviewActor)]
    assert len(previews) == 6
    for actor in previews:
        component = actor.skeletal_mesh_component
        assert component.get_skeletal_mesh_asset() == candidate
        assert component.get_editor_property('forced_lod_model') == 2
        animation = component.get_editor_property('animation_data')
        assert animation.anim_to_play and animation.saved_playing and animation.saved_looping
        assert all(isinstance(component.get_material(i), unreal.MaterialInstanceConstant)
                   for i in range(component.get_num_materials()))
    # Verify source gallery, lighting maps and persistent material references too.
    REPORT.update(candidate_persistence='PASS', mesh=data['mesh'], source_unchanged=True,
                  skeleton_and_physics_preserved=True, lod_vertices=[lod['vertices'] for lod in data['lods']])
    runpy.run_path(str(ROOT/'Tools/Validation/ValidateToonLightingAssets.py'), run_name='__main__')
    REPORT.update(status='PASS', original_lighting_gallery_regression='PASS')
except Exception:
    REPORT.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (ROOT/'Saved/ToonTest/performance_lod_validation.json').write_text(json.dumps(REPORT, indent=2), encoding='utf-8')
if REPORT['status'] != 'PASS':
    raise RuntimeError('Toon LOD persistence/regression failed')
