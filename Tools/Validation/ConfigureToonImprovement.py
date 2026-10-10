"""Transactional P0 master upgrade; preserves instance overrides and master defaults."""
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

RUN = Path(os.environ['PG_TOON_UPGRADE_RUN'])
LIB = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
OUTLINE = '/Game/Art/ToonTest/Advanced/Materials/M_PGToonScreenOutline'
REPORT = dict(status='RUNNING', run=str(RUN), masters=[])


def values(material):
    result = {}
    for kind in ['scalar', 'vector', 'texture']:
        result[kind] = {}
        for name in getattr(LIB, 'get_'+kind+'_parameter_names')(material):
            value = getattr(LIB, 'get_material_default_'+kind+'_parameter_value')(material, name)
            if kind == 'vector': value = [value.r, value.g, value.b, value.a]
            if kind == 'texture': value = value.get_path_name() if value else None
            result[kind][str(name)] = value
    return result


def restore_defaults(material, defaults):
    for node in LIB.get_material_expressions(material):
        kind = ('scalar' if isinstance(node, unreal.MaterialExpressionScalarParameter) else
                'vector' if isinstance(node, unreal.MaterialExpressionVectorParameter) else
                'texture' if isinstance(node, unreal.MaterialExpressionTextureSampleParameter2D) else None)
        if not kind: continue
        name = str(node.get_editor_property('parameter_name'))
        if name not in defaults[kind]: continue
        value = defaults[kind][name]
        if kind == 'vector': value = unreal.LinearColor(*value)
        if kind == 'texture': value = unreal.load_asset(value) if value else None
        node.set_editor_property('texture' if kind == 'texture' else 'default_value', value)
    LIB.recompile_material(material)
    shared.save(material)


def check(material, row):
    current = values(material)
    for kind, parameters in row['defaults'].items():
        for name, expected in parameters.items():
            actual = current[kind].get(name)
            if kind == 'scalar': assert abs(actual-expected) < 1e-5, (row['path'], name, actual, expected)
            elif kind == 'vector': assert max(abs(a-b) for a,b in zip(actual,expected)) < 1e-5, name
            else: assert actual == expected, name
    nodes = LIB.get_material_expressions(material)
    if row['path'] == OUTLINE:
        code = '\n'.join(n.get_editor_property('code') for n in nodes if isinstance(n,unreal.MaterialExpressionCustom))
        assert 'View.ViewResolutionFraction' in code and 'OverlapStrength' in code
    else:
        custom = [n for n in nodes if isinstance(n,unreal.MaterialExpressionCustom) and 'ToonDiffuse =' in n.get_editor_property('code')]
        assert len(custom) == 1, (row['path'], [(n.get_path_name(),n.get_editor_property('code')[:120]) for n in nodes if isinstance(n,unreal.MaterialExpressionCustom)])
        assert {'ToonDiffuse','ToonAccent'} <= set(LIB.get_material_expression_output_names(custom[0]))
        if row['world_lit']:
            node = LIB.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
            assert node.get_editor_property('code') == 'return Diffuse * saturate(Influence);'
            node = LIB.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            assert 'Accent + StateColor * StateGlow;' in node.get_editor_property('code')
    return dict(path=row['path'], status='PASS', expressions=len(nodes))


def main():
    validate = '-PGToonValidate' in unreal.SystemLibrary.get_command_line()
    manifest = RUN/'masters.json'
    if validate:
        rows = json.loads(manifest.read_text(encoding='utf-8'))
    else:
        rows = []
        for path in EAL.list_assets('/Game/Art/ToonTest', recursive=True):
            if '/Improvement/Native/' in path: continue
            if not path.rsplit('/',1)[-1].startswith('M_'): continue
            material = unreal.load_asset(path)
            if not isinstance(material, unreal.Material): continue
            names = {str(n) for n in LIB.get_scalar_parameter_names(material)}
            if not {'ShadeStrength','RimStrength','StateGlow'} <= names: continue
            defaults = values(material)
            rows.append(dict(path=material.get_path_name().split('.')[0], defaults=defaults,
                extended='AlphaMaskMode' in names,
                translucent=material.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT,
                world_lit=material.get_editor_property('shading_model') == unreal.MaterialShadingModel.MSM_DEFAULT_LIT,
                sdf=defaults['texture'].get('FaceSDFTexture')))
        assert len(rows) >= 9, len(rows)
        if os.environ.get('PG_TOON_SURFACE_ONLY') != '1':
            outline = unreal.load_asset(OUTLINE)
            assert outline
            rows.append(dict(path=OUTLINE, defaults=values(outline)))
        write_json(manifest, rows)
        tx = Transaction(ROOT, RUN)
        tx.prepare([r['path'] for r in rows], manifest)
        # Master upgrades must not rewrite any per-character instance or mesh.
        protected = {str(p.relative_to(ROOT)):sha256(p) for p in (ROOT/'Content/Art').rglob('*.uasset')
                     if p.name.startswith(('MI_', 'SK_'))}
        write_json(RUN/'protected.json',protected)
        for row in rows:
            path = row['path']
            tx.mark_written(path)
            shared.MASTER_DEST = path.rsplit('/',1)[0]
            if path == OUTLINE:
                from ConfigureToonLightingLab import screen_outline
                material = screen_outline()
            else:
                material = shared.build_toon_master(path.rsplit('/',1)[1], row['extended'], row['translucent'],
                    row['world_lit'], unreal.load_asset(row['sdf']) if row['sdf'] else None)
            restore_defaults(material,row['defaults'])
        tx.data['status']='WRITTEN'
        tx.flush()
    for row in rows: REPORT['masters'].append(check(unreal.load_asset(row['path']),row))
    for path,digest in json.loads((RUN/'protected.json').read_text()).items():
        assert sha256(ROOT/path) == digest, 'Unexpected character asset modification: '+path
    REPORT['status']='PASS'


try:
    main()
except BaseException:
    REPORT.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    step = 'validate' if '-PGToonValidate' in unreal.SystemLibrary.get_command_line() else 'apply'
    write_json(RUN/(step+'.json'),REPORT)
if REPORT['status'] != 'PASS': raise RuntimeError(REPORT.get('error'))
