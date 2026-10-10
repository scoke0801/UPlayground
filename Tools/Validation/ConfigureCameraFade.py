"""Add camera fade to existing toon/hull graphs without rebuilding authored shading."""
import json
import os
from pathlib import Path
import sys
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
import ConfigureToonCharacterTest as shared
from PlayableCharacterTransaction import Transaction, write_json, sha256

OUT = Path(os.environ['PG_CAMERA_FADE_RUN'])
LIB = unreal.MaterialEditingLibrary
verify = '-PGCameraFadeVerify' in unreal.SystemLibrary.get_command_line()
report = dict(status='RUNNING', materials=[])
SWORD_MI = '/Game/ExternalAssets/LevelDesign/Dungeon_Pack/Assets/Pack_Characters/Clothing/Props/Materials/MI_Sword-Dagger'
SWORD_MASTER = '/Game/Art/ToonTest/Materials/M_PGCameraFadeSword'
try:
    if verify:
        rows = json.loads((OUT/'materials.json').read_text())
    else:
        rows = []
        for path in unreal.EditorAssetLibrary.list_assets('/Game/Art/ToonTest', recursive=True):
            if '/Native/' in path or not path.rsplit('/', 1)[-1].startswith('M_'):
                continue
            material = unreal.load_asset(path)
            if not isinstance(material, unreal.Material):
                continue
            names = {str(n) for n in LIB.get_scalar_parameter_names(material)}
            if not ({'ShadeStrength', 'StateGlow'} <= names or 'OutlineWidth' in names):
                continue
            transparent = material.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT
            prop = unreal.MaterialProperty.MP_OPACITY if transparent else unreal.MaterialProperty.MP_OPACITY_MASK
            assert LIB.get_material_property_input_node(material, prop), path
            rows.append(dict(path=material.get_path_name().split('.')[0], transparent=transparent))
        assert len(rows) >= 15, rows
        if not any(r['path'] == SWORD_MASTER for r in rows):
            rows.append(dict(path=SWORD_MASTER, transparent=False, equipment=True))
        write_json(OUT/'materials.json', rows)
        protected = {str(p.relative_to(ROOT)): sha256(p) for p in (ROOT/'Content/Art').rglob('*.uasset')
                     if p.name.startswith(('MI_', 'SK_'))}
        write_json(OUT/'protected.json', protected)
        tx = Transaction(ROOT, OUT)
        tx.prepare([r['path'] for r in rows] + [SWORD_MI], OUT/'materials.json')
        for row in rows:
            material = unreal.load_asset(row['path']) if unreal.EditorAssetLibrary.does_asset_exist(row['path']) else None
            if row.get('equipment'):
                instance = unreal.load_asset(SWORD_MI)
                assert instance
                if not material:
                    original = instance.get_editor_property('parent')
                    while isinstance(original, unreal.MaterialInstance):
                        original = original.get_editor_property('parent')
                    assert isinstance(original, unreal.Material)
                    assert not original.get_editor_property('use_material_attributes'), 'Equipment uses material attributes'
                    tx.mark_written(row['path'])
                    material = unreal.EditorAssetLibrary.duplicate_asset(original.get_path_name(), row['path'])
                    assert material
                tx.mark_written(SWORD_MI)
                LIB.set_material_instance_parent(instance, material)
                shared.save(instance)
            if 'CameraFadeAmount' in {str(n) for n in LIB.get_scalar_parameter_names(material)}:
                if row.get('equipment'):
                    # The source sword was Opaque: even its authored mask pin was ignored.
                    # Do not turn dark BaseColour (function output 0) into opacity on migration.
                    output = LIB.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY_MASK)
                    source = LIB.get_inputs_for_material_expression(material, output)[0]
                    if not isinstance(source, unreal.MaterialExpressionConstant) or source.get_editor_property('r') != 1.:
                        tx.mark_written(row['path'])
                        solid = shared.expression(material, unreal.MaterialExpressionConstant, -600, 1900)
                        solid.set_editor_property('r', 1.)
                        assert LIB.connect_material_expressions(solid, '', output, 'Opacity')
                        LIB.recompile_material(material)
                        shared.save(material)
                continue
            prop = unreal.MaterialProperty.MP_OPACITY if row['transparent'] else unreal.MaterialProperty.MP_OPACITY_MASK
            opacity = LIB.get_material_property_input_node(material, prop)
            opacity_output = LIB.get_material_property_input_node_output_name(material, prop)
            tx.mark_written(row['path'])
            if row.get('equipment') or opacity is None:
                assert row.get('equipment')
                opacity = shared.expression(material, unreal.MaterialExpressionConstant, -600, 1900)
                opacity.set_editor_property('r', 1.)
                opacity_output = ''
            if row.get('equipment'):
                material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
            shared.connect_camera_fade(material, opacity, row['transparent'], opacity_output)
            LIB.recompile_material(material)
            shared.save(material)
        tx.data['status'] = 'WRITTEN'
        tx.flush()
    for row in rows:
        material = unreal.load_asset(row['path'])
        nodes = LIB.get_material_expressions(material)
        fade = [n for n in nodes if isinstance(n, unreal.MaterialExpressionScalarParameter)
                and str(n.get_editor_property('parameter_name')) == 'CameraFadeAmount']
        assert len(fade) == 1 and fade[0].get_editor_property('use_custom_primitive_data')
        assert fade[0].get_editor_property('primitive_data_index') == 7
        prop = unreal.MaterialProperty.MP_OPACITY if row['transparent'] else unreal.MaterialProperty.MP_OPACITY_MASK
        output = LIB.get_material_property_input_node(material, prop)
        assert 'IsShadowDepthShader' in output.get_editor_property('code')
        if row.get('equipment'):
            source = LIB.get_inputs_for_material_expression(material, output)[0]
            assert isinstance(source, unreal.MaterialExpressionConstant) and source.get_editor_property('r') == 1., 'Opaque sword must preserve full coverage before camera fade'
        report['materials'].append(row['path'])
    for path, digest in json.loads((OUT/'protected.json').read_text()).items():
        assert sha256(ROOT/path) == digest, path
    assert unreal.load_asset(SWORD_MI).get_editor_property('parent') == unreal.load_asset(SWORD_MASTER)
    report['status'] = 'PASS'
except BaseException:
    report.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(report['error'])
finally:
    write_json(OUT/('verify.json' if verify else 'apply.json'), report)
if report['status'] != 'PASS':
    raise RuntimeError(report['error'])
