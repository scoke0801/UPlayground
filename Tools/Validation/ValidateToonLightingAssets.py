"""Fresh-process saved asset validation, followed by original gallery regression."""
import json
import math
import runpy
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
REPORT = {'status': 'RUNNING', 'characters': [], 'maps': []}
OUTPUT = ROOT / 'Saved/ToonTest/lighting_validation.json'
LIB = unreal.MaterialEditingLibrary


def main():
    data = json.loads((ROOT / 'Saved/ToonTest/lighting_lab.json').read_text())
    assert data['status'] == 'PASS'
    for row in data['characters']:
        mesh = unreal.load_asset(row['mesh'])
        slots = mesh.get_editor_property('materials')
        assert len(slots) == len(row['materials'])
        for slot, config in zip(slots, row['materials']):
            assert str(slot.material_slot_name) == config['slot']
            mi = unreal.load_asset(config['asset'])
            assert isinstance(mi, unreal.MaterialInstanceConstant)
            assert slot.material_interface != mi, 'Original mesh was overwritten by lab variant'
            parent = mi.get_editor_property('parent')
            assert parent.get_path_name().startswith('/Game/Art/ToonTest/Advanced/Materials/')
            assert parent.get_editor_property('shading_model') == (unreal.MaterialShadingModel.MSM_UNLIT if config['overlay'] else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
            assert LIB.get_material_instance_texture_parameter_value(mi, 'BaseTexture')
            for param in ['StateGlow', 'DissolveAmount']:
                assert abs(LIB.get_material_instance_scalar_parameter_value(mi, param)) < .0001
            if not config['overlay']:
                face = config['profile'] in ['face', 'detail']
                assert abs(LIB.get_material_instance_scalar_parameter_value(mi, 'FaceShading') - (.85 if face else 0)) < .0001
                if parent.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_MASKED:
                    assert abs(LIB.get_material_instance_scalar_parameter_value(mi, 'ShadowCast') - (0 if face else 1)) < .0001
                assert abs(LIB.get_material_instance_scalar_parameter_value(mi, 'HairAnisotropy') - (.8 if config['profile'] == 'hair' else 0)) < .0001
                influence = LIB.get_material_instance_scalar_parameter_value(mi, 'WorldLightingInfluence')
                assert 0 < influence <= 1
        REPORT['characters'].append({'name': row['name'], 'slots': len(slots)})
    outline = unreal.load_asset(data['outline'])
    assert outline.get_editor_property('material_domain') == unreal.MaterialDomain.MD_POST_PROCESS
    assert outline.get_editor_property('blendable_location') == unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF
    mode = unreal.load_asset(data['game_mode']).generated_class()
    assert unreal.get_default_object(mode).get_editor_property('default_pawn_class') is None
    for path, expected in [('/Game/Art/ToonTest/Maps/L_PGToon_LightingLab', 4),
                           ('/Game/Art/ToonTest/Maps/L_PGToon_AdvancedMotionTest', 6)]:
        world = unreal.EditorLoadingAndSavingUtils.load_map(path)
        assert world and world.get_world_settings().get_editor_property('default_game_mode') == mode
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        bases = [a for a in actors if isinstance(a, unreal.PGToonPreviewActor)]
        assert len(bases) == expected
        for a in bases:
            c, p = a.skeletal_mesh_component, a.toon_presentation
            assert p.key_light and c.does_socket_exist(p.head_bone)
            for v in [p.head_forward_axis, p.head_right_axis]:
                assert all(math.isfinite(x) for x in [v.x, v.y, v.z])
                assert abs(v.x*v.x+v.y*v.y+v.z*v.z-1) < .001
            assert c.get_editor_property('render_custom_depth')
            assert c.get_editor_property('custom_depth_stencil_value') == 73
            assert all(isinstance(c.get_material(i), unreal.MaterialInstanceConstant) for i in range(c.get_num_materials())), 'Transient MID saved in fixture'
            if expected == 6:
                animation = c.get_editor_property('animation_data')
                assert animation.anim_to_play and animation.saved_playing and animation.saved_looping
                assert animation.saved_play_rate == 1
        assert len([a for a in actors if isinstance(a, unreal.SkeletalMeshActor)]) == expected, 'Extra hull duplicate'
        REPORT['maps'].append({'path': path, 'actors': expected, 'persistent_materials': True})
    runpy.run_path(str(ROOT/'Tools/Validation/ValidateToonCharacterGallery.py'), run_name='__main__')
    REPORT.update(status='PASS', slots=41, original_gallery_regression='PASS')


try:
    main()
except Exception:
    REPORT.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    OUTPUT.write_text(json.dumps(REPORT, indent=2), encoding='utf-8')
if REPORT['status'] != 'PASS':
    raise RuntimeError('Toon lighting assets validation failed: '+str(OUTPUT))
