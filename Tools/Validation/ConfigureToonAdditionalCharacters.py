"""Import the requested three Unity characters, preserving the Inori prototype.

Unity GUIDs resolve within each source package; no textures are guessed by name.
Only materials actually used by imported mesh slots are converted. Original Unity
files are read-only; regenerated Unreal prototype folders are backed up.
"""
import hashlib
import json
import re
import shutil
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT / 'Tools/Validation'))
import ConfigureToonCharacterTest as shared

CONFIG = json.loads((ROOT / 'Tools/Art/ToonTest/characters.json').read_text(encoding='utf-8'))
OUT = ROOT / 'Saved/ToonTest/AdditionalImport' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
LATEST = ROOT / 'Saved/ToonTest/additional_import.json'
REPORT = {'status': 'RUNNING', 'run': str(OUT), 'characters': []}
LATEST.write_text(json.dumps(REPORT), encoding='utf-8')
LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


from ToonMaterialSource import parse_material, shader_catalog

SHADERS = shader_catalog(Path(CONFIG['unity_root']).parents[2])


def canonical(name):
    return re.sub(r'[^a-z0-9]', '', name.lower().replace('(instance)', ''))


def backup(relative):
    folder = ROOT / 'Content/Art/ToonTest' / relative
    if folder.is_dir():
        shutil.copytree(folder, OUT / 'backup' / relative)


def import_character(spec, master, transparent_master):
    name = spec['name']
    source = Path(CONFIG['unity_root']) / spec['folder']
    fbx = source / spec['fbx']
    assert fbx.is_file(), fbx
    backup(name)
    guids = {}
    for meta in source.rglob('*.meta'):
        match = re.search(r'^guid: ([a-f0-9]{32})$', meta.read_text(encoding='utf-8-sig'), re.M)
        if match:
            guids[match[1]] = Path(str(meta)[:-5])
    materials = [parse_material(p, guids, SHADERS) for p in source.rglob('*.mat')]
    material_lookup = {canonical(m['name']): m for m in materials}
    shared.source_root = source
    shared.destination = '/Game/Art/ToonTest/' + name
    shared.mesh_path = shared.destination + '/SK_' + name + '_ToonTest'
    shared.manifest = {'fbx': spec['fbx'], 'skeletal_mesh': 'SK_' + name + '_ToonTest'}
    source_hashes = {str(fbx): digest(fbx)}
    mesh = unreal.load_asset(shared.mesh_path) if unreal.EditorAssetLibrary.does_asset_exist(shared.mesh_path) else shared.import_mesh()
    slots = list(mesh.get_editor_property('materials'))
    row = {'name': name, 'mesh': mesh.get_path_name(), 'slots': [], 'unmatched_slots': []}
    textures = {}
    for index, slot in enumerate(slots):
        slot_name = str(slot.material_slot_name)
        key = canonical(slot_name)
        config = material_lookup.get(key)
        if not config:
            row['unmatched_slots'].append(slot_name)
            continue
        assert config['texture'] or not config['texture_guid'], 'Unresolved base texture GUID: ' + config['source_material']
        source_hashes[config['source_material']] = digest(Path(config['source_material']))
        def texture(path):
            if path not in textures:
                source_hashes[path] = digest(Path(path))
                asset_name = 'T_' + re.sub(r'[^A-Za-z0-9_]', '_', Path(path).stem)
                asset_path = shared.destination + '/Textures/' + asset_name
                textures[path] = unreal.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else shared.import_texture(Path(path))
            return textures[path]
        base_texture = texture(config['texture']) if config['texture'] else unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture')
        instance_name = 'MI_PGToon_' + name + '_' + re.sub(r'[^A-Za-z0-9_]', '_', slot_name)
        instance_path = shared.destination + '/Materials/' + instance_name
        mi = unreal.load_asset(instance_path) if unreal.EditorAssetLibrary.does_asset_exist(instance_path) else TOOLS.create_asset(instance_name, shared.destination+'/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        LIB.set_material_instance_parent(mi, transparent_master if config['transparent'] else master)
        LIB.set_material_instance_texture_parameter_value(mi, 'BaseTexture', base_texture)
        LIB.set_material_instance_scalar_parameter_value(mi, 'OpacityCutoff', config['cutoff'])
        LIB.set_material_instance_scalar_parameter_value(mi, 'MainOpacity', config['main_opacity'])
        LIB.set_material_instance_vector_parameter_value(mi, 'BaseTint', unreal.LinearColor(*config['tint']))
        for param, value in [('UseBaseAlpha', config['use_base_alpha']), ('AlphaMaskMode', config['alpha_mode']), ('AlphaMaskScale', config['alpha_scale']), ('AlphaMaskValue', config['alpha_value'])]:
            LIB.set_material_instance_scalar_parameter_value(mi, param, value)
        if config['opacity_texture']:
            opacity_texture = texture(config['opacity_texture'])
            if config['opacity_texture'] != config['texture']:
                opacity_texture.set_editor_property('srgb', False)
                shared.save(opacity_texture)
            LIB.set_material_instance_texture_parameter_value(mi, 'OpacityTexture', opacity_texture)
        shared.apply_shading_profile(mi, name, slot_name)
        shared.save(mi)
        slot.set_editor_property('material_interface', mi)
        slots[index] = slot
        row['slots'].append(dict(config, slot=slot_name, instance=instance_path))
    mesh.set_editor_property('materials', slots)
    shared.save(mesh)
    row.update(texture_count=len(textures), skeleton=mesh.get_editor_property('skeleton').get_path_name(),
               source_hashes=source_hashes, sources_unchanged=all(digest(Path(p)) == h for p, h in source_hashes.items()),
               status='PASS' if not row['unmatched_slots'] else 'FAIL')
    return row


try:
    # New extended masters leave Inori's existing materials and reports untouched.
    backup('Materials')
    master = shared.build_toon_master('M_PGToonCharacterMulti', extended_alpha=True)
    transparent_master = shared.build_toon_master('M_PGToonCharacterMultiTransparent', extended_alpha=True, translucent=True)
    shared.build_outline_master('M_PGToonOutlineMulti', extended_alpha=True)
    for spec in CONFIG['characters']:
        try:
            REPORT['characters'].append(import_character(spec, master, transparent_master))
        except Exception:
            REPORT['characters'].append({'name': spec['name'], 'status': 'FAIL', 'error': traceback.format_exc()})
    REPORT['status'] = 'PASS' if all(c['status'] == 'PASS' for c in REPORT['characters']) else 'FAIL'
except Exception:
    REPORT.update(status='FAIL', error=traceback.format_exc())
finally:
    payload = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (OUT / 'import.json').write_text(payload, encoding='utf-8')
    LATEST.write_text(payload, encoding='utf-8')
if REPORT['status'] != 'PASS':
    raise RuntimeError('Additional Toon import failed: ' + str(LATEST))
unreal.log('PG additional toon characters PASS')
