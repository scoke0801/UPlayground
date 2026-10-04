"""Read-only fresh-process validation of gallery assets and persisted references."""
import hashlib
import json
import math
import runpy
import sys
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT / 'Tools/Validation'))
from ConfigureToonCharacterTest import shading_profile
REPORT = {'status': 'RUNNING', 'characters': []}
OUTPUT = ROOT / 'Saved/ToonTest/gallery_validation.json'
AUDIT = json.loads((ROOT / 'Saved/ToonTest/additional_import.json').read_text(encoding='utf-8'))
MAP = '/Game/Art/ToonTest/Maps/L_PGToon_CharacterGallery'


def main():
    assert AUDIT['status'] == 'PASS'
    hashes_checked = 0
    for row in AUDIT['characters']:
        assert row['status'] == 'PASS' and not row['unmatched_slots']
        for path, expected in row['source_hashes'].items():
            assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == expected, path
            hashes_checked += 1
        mesh = unreal.load_asset(row['mesh'])
        assert isinstance(mesh, unreal.SkeletalMesh)
        assert mesh.get_editor_property('skeleton') == unreal.load_asset(row['skeleton'])
        slots = mesh.get_editor_property('materials')
        assert len(slots) == len(row['slots'])
        for slot, config in zip(slots, row['slots']):
            assert str(slot.material_slot_name) == config['slot']
            mi = slot.material_interface
            assert mi == unreal.load_asset(config['instance'])
            parent = mi.get_editor_property('parent')
            suffix = 'Transparent' if config['transparent'] else ''
            assert parent == unreal.load_asset('/Game/Art/ToonTest/Materials/M_PGToonCharacterMulti'+suffix)
            assert unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mi, 'BaseTexture')
            lib = unreal.MaterialEditingLibrary
            if 'render_mode' in config:
                assert config['shader_resolved'], config['source_material']
                for param, expected in [('UseBaseAlpha', config['use_base_alpha']), ('MainOpacity', config['main_opacity'])]:
                    assert abs(lib.get_material_instance_scalar_parameter_value(mi, param)-expected) < 1e-5, (config['slot'], param)
                profile = shading_profile(row['name'], config['slot'])
                for param, expected in profile['scalars'].items():
                    assert abs(lib.get_material_instance_scalar_parameter_value(mi, param)-expected) < 1e-5, (config['slot'], param)
                for param, expected in profile['vectors'].items():
                    actual = lib.get_material_instance_vector_parameter_value(mi, param)
                    assert all(abs(getattr(actual, axis)-v) < 1e-5 for axis, v in zip(['r', 'g', 'b', 'a'], expected)), (config['slot'], param)
            if config['opacity_texture']:
                alpha = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mi, 'OpacityTexture')
                assert alpha
                if config['opacity_texture'] != config['texture']:
                    assert not alpha.get_editor_property('srgb'), alpha.get_path_name()
        REPORT['characters'].append({'name': row['name'], 'slots_verified': len(slots),
                                      'texture_count': row['texture_count'], 'skeleton': row['skeleton']})
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    skeletal = {a.get_actor_label(): a for a in actors if isinstance(a, unreal.SkeletalMeshActor)}
    assert len(skeletal) == 8
    for name in ['Inori', 'Bokusei', 'Honoka', 'LianLian']:
        base = skeletal['PG Toon Gallery '+name]
        hull = skeletal['PG Toon Gallery '+name+' Outline']
        component = base.skeletal_mesh_component
        follower = hull.skeletal_mesh_component
        assert follower.get_editor_property('leader_pose_component') == component
        assert follower.get_num_materials() == component.get_num_materials()
        assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        assert all(component.get_material(i) and follower.get_material(i) for i in range(component.get_num_materials()))
        if name != 'Inori' and 'render_mode' in AUDIT['characters'][0]['slots'][0]:
            for i in range(component.get_num_materials()):
                for param in ['UseBaseAlpha', 'MainOpacity', 'AlphaMaskMode', 'OpacityCutoff', 'DissolveAmount']:
                    assert abs(lib.get_material_instance_scalar_parameter_value(component.get_material(i), param) -
                               lib.get_material_instance_scalar_parameter_value(follower.get_material(i), param)) < 1e-5, (name, i, param)
        origin, extent = base.get_actor_bounds(False)
        assert all(math.isfinite(v) for v in [origin.x, origin.y, origin.z, extent.x, extent.y, extent.z])
        assert extent.z > 0
        REPORT.setdefault('foot_bones', {})[name] = {
            str(bone): [getattr(component.get_socket_location(bone), axis) for axis in ['x', 'y', 'z']]
            for bone in [component.get_bone_name(i) for i in range(component.get_num_bones())]
            if any(part in str(bone).lower() for part in ['foot', 'toe'])
        }
        if name == 'Inori':
            data = component.get_editor_property('animation_data')
            assert data.anim_to_play and data.saved_playing and data.saved_looping
            assert abs(data.saved_play_rate-1) < .0001
    honoka_toes = [pos[2] for bone, pos in REPORT['foot_bones']['Honoka'].items() if bone.lower().startswith('toes')]
    assert len(honoka_toes) == 2 and all(2 < z < 4 for z in honoka_toes), honoka_toes
    camera = next(a for a in actors if isinstance(a, unreal.CameraActor))
    assert camera.get_auto_activate_player_index() == 0
    assert abs(camera.camera_component.field_of_view-60) < .001
    upgrade_path = ROOT / 'Saved/ToonTest/shading_upgrade.json'
    if upgrade_path.is_file():
        upgrade = json.loads(upgrade_path.read_text(encoding='utf-8'))
        assert upgrade['status'] == 'PASS', 'Latest shading upgrade failed'
        for path, expected in upgrade['protected_hashes'].items():
            assert hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == expected, path
        REPORT['shading_profiles_and_alpha'] = 'PASS'
    # Also reload the untouched Inori motion map and check its original source hashes.
    runpy.run_path(str(ROOT / 'Tools/Validation/ValidateToonMotionTest.py'), run_name='__main__')
    REPORT.update(status='PASS', map=MAP, source_files_unchanged=hashes_checked,
                  base_actors=4, outline_leaders_persisted=4, inori_motion_regression='PASS',
                  notes='Asset/reference validation; new models remain in reference pose. Not gameplay or deformation QA.')


try:
    main()
except Exception:
    REPORT.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    OUTPUT.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
if REPORT['status'] != 'PASS':
    raise RuntimeError('Toon gallery validation failed: '+str(OUTPUT))
unreal.log('PG Toon character gallery validation PASS')
