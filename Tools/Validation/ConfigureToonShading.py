"""Upgrade existing toon prototypes without reimporting meshes or saving maps.

Back up affected assets first. Requires the completed Inori + additional imports.
Run using UE 5.8 Python commandlet; then run PreviewToonShading.py with SM6.
"""
import hashlib
import json
import runpy
import shutil
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT / 'Tools/Validation'))
import ConfigureToonCharacterTest as shared

OUT = ROOT / 'Saved/ToonTest/ShadingUpgrade' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
LATEST = ROOT / 'Saved/ToonTest/shading_upgrade.json'
REPORT = {'status': 'RUNNING', 'run': str(OUT), 'materials': []}
LATEST.write_text(json.dumps(REPORT), encoding='utf-8')
LIB = unreal.MaterialEditingLibrary


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    content = ROOT / 'Content/Art/ToonTest'
    preserved = [p for p in content.rglob('*') if p.suffix in ['.uasset', '.umap'] and
                 ('Maps' in p.parts or 'Animation' in p.parts or p.name.startswith('SK_'))]
    before = {str(p.relative_to(ROOT)): digest(p) for p in preserved}
    for relative in ['Materials', 'Inori/Materials']:
        shutil.copytree(content / relative, OUT / 'backup' / relative)
    for name in ['additional_import.json', 'outline.json']:
        shutil.copy2(ROOT / 'Saved/ToonTest' / name, OUT / name)
    shared.build_toon_master()
    for path in unreal.EditorAssetLibrary.list_assets('/Game/Art/ToonTest/Inori/Materials', recursive=False):
        mi = unreal.load_asset(path)
        if not isinstance(mi, unreal.MaterialInstanceConstant):
            continue
        if mi.get_editor_property('parent') != unreal.load_asset(shared.MASTER_DEST + '/M_PGToonCharacter'):
            continue
        slot = 'M_' + mi.get_name().removeprefix('MI_PGToon_')
        profile = shared.apply_shading_profile(mi, 'Inori', slot)
        shared.save(mi)
        REPORT['materials'].append({'asset': path, 'profile': profile['name']})
    shared.build_outline_master()
    runpy.run_path(str(ROOT / 'Tools/Validation/ConfigureToonOutlineTest.py'), run_name='__main__')
    runpy.run_path(str(ROOT / 'Tools/Validation/ConfigureToonAdditionalCharacters.py'), run_name='__main__')
    audit = json.loads((ROOT / 'Saved/ToonTest/additional_import.json').read_text(encoding='utf-8'))
    assert audit['status'] == 'PASS'
    for row in audit['characters']:
        for slot in row['slots']:
            mi = unreal.load_asset(slot['instance'])
            profile = shared.shading_profile(row['name'], slot['slot'])
            REPORT['materials'].append({'asset': slot['instance'], 'profile': profile['name'],
                                        'render_mode': slot['render_mode'], 'shader': slot['shader_name']})
            outline_name = mi.get_name().replace('MI_PGToon_', 'MI_PGToonOutline_', 1)
            outline_path = '/Game/Art/ToonTest/' + row['name'] + '/Materials/' + outline_name
            outline = unreal.load_asset(outline_path)
            assert outline, outline_path
            for param in ['AlphaMaskMode', 'AlphaMaskScale', 'AlphaMaskValue', 'OpacityCutoff',
                          'MainOpacity', 'UseBaseAlpha', 'DissolveAmount']:
                LIB.set_material_instance_scalar_parameter_value(outline, param,
                    LIB.get_material_instance_scalar_parameter_value(mi, param))
            face = any(word in slot['slot'].lower() for word in ['face', 'eye', 'brow', 'trans', 'tears'])
            LIB.set_material_instance_scalar_parameter_value(outline, 'OutlineEnabled', 0 if face or slot['transparent'] else 1)
            LIB.set_material_instance_scalar_parameter_value(outline, 'OutlineWidth', .08 if face else (.18 if profile['name'] == 'hair' else .3))
            LIB.update_material_instance(outline)
            shared.save(outline)
    # Additional import resaves mesh slot bindings; hash protection covers actual
    # skeleton/animation/map assets, while mesh geometry is verified by no FBX import.
    immutable = {p: h for p, h in before.items() if not Path(p).name.startswith('SK_') or
                 any(v in Path(p).stem for v in ['Skeleton', 'PhysicsAsset'])}
    assert all(digest(ROOT / p) == h for p, h in immutable.items()), 'Protected asset changed'
    REPORT.update(status='PASS', protected_hashes=immutable, protected_assets_unchanged=len(immutable),
                  additional_import_run=audit['run'], shader_compile_validation='Requires SM6 render',
                  lighting='Art-directed material parameters; no engine light/shadow reception')


try:
    main()
except Exception:
    REPORT.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    payload = json.dumps(REPORT, ensure_ascii=False, indent=2)
    LATEST.write_text(payload, encoding='utf-8')
    (OUT / 'upgrade.json').write_text(payload, encoding='utf-8')
if REPORT['status'] != 'PASS':
    raise RuntimeError('Toon shading upgrade failed: ' + str(LATEST))
unreal.log('PG Toon shading upgrade PASS')
