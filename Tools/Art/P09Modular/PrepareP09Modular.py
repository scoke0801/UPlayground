"""Read Unity P09 sources and export separate FBXs using background Blender.

The source project is read-only. Run with Blender --background --python this_file.
Preserves rig/morph data; does not translate Unity physics or constraint solvers.
"""
import hashlib
import json
import re
import sys
from pathlib import Path

import bpy

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
SOURCE = Path('C:/UsingProject/UnityProject/UPlayground/Assets/ExternalAssets/Character/P09_Modular_Humanoid')
sys.path.insert(0, str(ROOT / 'Tools/Validation'))
from ToonMaterialSource import parse_material, shader_catalog


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def blocks(path):
    return re.findall(r'^--- !u!(\d+) &(-?\d+)[^\n]*\n(.*?)(?=^--- !u!|\Z)',
                      path.read_text(encoding='utf-8-sig'), re.M | re.S)


def field(text, key):
    m = re.search(r'^  ' + re.escape(key) + r': (.*)$', text, re.M)
    return m[1].strip() if m else None


def ref(text, key):
    m = re.search(r'^  ' + re.escape(key) + r': \{fileID: (-?\d+)', text, re.M)
    return m[1] if m else None


def prefab_data(guids):
    base = SOURCE / 'Model_DATA/Prefab/No_MagicaCloth/P09_Human_No_Physics.prefab'
    gos, transforms, renderers = {}, {}, []
    for kind, oid, text in blocks(base):
        if kind == '1':
            gos[oid] = dict(name=field(text, 'm_Name'), active=field(text, 'm_IsActive') == '1')
        elif kind == '4' and field(text, 'm_Father'):
            transforms[oid] = dict(go=ref(text, 'm_GameObject'), parent=ref(text, 'm_Father'))
        elif kind in ('137', '23'):
            materials = re.search(r'^  m_Materials:\n(.*?)(?=^  \w)', text, re.M | re.S)
            material_guids = re.findall(r'guid: ([a-f0-9]{32})', materials[1]) if materials else []
            renderers.append(dict(id=oid, go=ref(text, 'm_GameObject'), materials=material_guids,
                                  mesh=field(text, 'm_Mesh'), root=ref(text, 'm_RootBone')))
    by_go = {t['go']: tid for tid, t in transforms.items()}
    def ancestors(go):
        result = [go]
        t = transforms.get(by_go.get(go))
        while t and t['parent'] in transforms:
            t = transforms[t['parent']]
            result.append(t['go'])
        return result
    for row in renderers:
        row['name'] = gos[row['go']]['name']
        row['path'] = '/'.join(gos[g]['name'] for g in reversed(ancestors(row['go'])))
        assert all(g in guids for g in row['materials']), row
    presets = {}
    for sex in ('Female', 'Male'):
        p = base.with_name('P09_Human_No_Physics_' + sex + ' Variant.prefab')
        text = p.read_text(encoding='utf-8-sig')
        active = {k: v['active'] for k, v in gos.items()}
        for go, value in re.findall(r'target: \{fileID: (-?\d+),[^\n]*\n\s+propertyPath: m_IsActive\n\s+value: ([01])', text):
            active[go] = value == '1'
        removed = re.search(r'm_RemovedGameObjects:\n(.*?)(?=    m_AddedGameObjects)', text, re.S)
        removed_ids = set(re.findall(r'fileID: (-?\d+)', removed[1])) if removed else set()
        selected = [r['name'] for r in renderers if all(active.get(g, False) and g not in removed_ids for g in ancestors(r['go']))]
        assert selected, sex
        presets[sex] = dict(source=str(p.relative_to(SOURCE)), parts=selected,
                            note='Renderer selection preserved; preview uses FBX reference pose, not Unity constraint pose.')
    return renderers, presets


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'FBX').mkdir(exist_ok=True)
    guids = {}
    for meta in SOURCE.rglob('*.meta'):
        m = re.search(r'^guid: ([a-f0-9]{32})$', meta.read_text(encoding='utf-8-sig'), re.M)
        if m:
            guids[m[1]] = Path(str(meta)[:-5])
    renderers, presets = prefab_data(guids)
    shaders = shader_catalog(SOURCE.parents[3])
    materials = {g: parse_material(p, guids, shaders) for g, p in guids.items() if p.suffix == '.mat'}
    by_name = {}
    for row in renderers:
        if row['name'] in by_name:
            assert row['materials'] == by_name[row['name']]['materials'], row['name']
        by_name[row['name']] = row
    manifest = dict(schema=1, source_root=str(SOURCE), destination='/Game/Art/P09Modular',
                    masters={'opaque':'/Game/Art/ToonTest/Advanced/Materials/M_PGToonWorld_multi',
                             'transparent':'/Game/Art/ToonTest/Advanced/Materials/M_PGToonWorld_transparent'},
                    presets=presets, parts=[], materials=materials, source_hashes={})
    for source in sorted((SOURCE / 'Model_DATA/FBX').glob('*.fbx')):
        manifest['source_hashes'][str(source)] = digest(source)
        meta = source.with_suffix('.fbx.meta')
        manifest['source_hashes'][str(meta)] = digest(meta)
        external_materials = dict(re.findall(r'      name: ([^\n]+)\n    second: \{fileID: 2100000, guid: ([a-f0-9]{32})', meta.read_text(encoding='utf-8-sig')))
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(source), use_anim=False)
        meshes = sorted((o for o in bpy.data.objects if o.type == 'MESH'), key=lambda o:o.name)
        for obj in meshes:
            armature = obj.find_armature()
            materials_fbx = [m.name if m else None for m in obj.data.materials]
            used = sorted({p.material_index for p in obj.data.polygons})
            # Unity drops unused FBX material slots. Match that compact submesh order.
            original_materials = list(obj.data.materials)
            indices = [p.material_index for p in obj.data.polygons]
            obj.data.materials.clear()
            for index in used:
                obj.data.materials.append(original_materials[index])
            for polygon, index in zip(obj.data.polygons, indices):
                polygon.material_index = used.index(index)
            materials_fbx = [materials_fbx[i] for i in used]
            row = by_name.get(obj.name) or by_name.get(obj.name.removeprefix('Weapon_'))
            # Unity's submesh order can differ from FBX/Blender. Never zip its
            # material array to FBX slots. Resolve the source importer's name->GUID
            # table, then verify multi-slot prefab overrides are the same set.
            mapped = [external_materials.get(n) or external_materials.get(re.sub(r'\.\d{3}$','',n)) for n in materials_fbx]
            if row and len(row['materials']) == 1 and len(materials_fbx) == 1:
                mapped = row['materials']  # A single-slot override is unambiguous.
            elif row:
                assert set(mapped) == set(row['materials']), (obj.name, mapped, row['materials'])
            assert len(mapped) == len(materials_fbx) and all(mapped), (obj.name, materials_fbx, mapped)
            name = ('SK_' if armature else 'SM_') + 'P09_' + re.sub(r'[^A-Za-z0-9_]', '_', obj.name)
            target = OUT / 'FBX' / (name + '.fbx')
            bpy.ops.object.select_all(action='DESELECT')
            obj.select_set(True)
            if armature: armature.select_set(True)
            bpy.context.view_layer.objects.active = obj
            bpy.ops.export_scene.fbx(filepath=str(target), use_selection=True, object_types={'MESH','ARMATURE'},
                add_leaf_bones=False, bake_anim=False, use_mesh_modifiers=False,
                mesh_smooth_type='FACE', axis_forward='-Y', axis_up='Z', path_mode='STRIP')
            manifest['parts'].append(dict(name=obj.name, asset=name, fbx=str(target.relative_to(ROOT)),
                source_fbx=str(source), armature=armature.name if armature else None,
                bone_count=len(armature.data.bones) if armature else 0,
                materials=mapped, fbx_material_slots=materials_fbx,
                prefab_path=row['path'] if row else None,
                morphs=[k.name for k in obj.data.shape_keys.key_blocks][1:] if obj.data.shape_keys else []))
    names = {p['name'] for p in manifest['parts']}
    assert len(names) == len(manifest['parts']), 'Duplicate mesh names across FBXs'
    for preset in presets.values():
        assert set(preset['parts']) <= names, set(preset['parts']) - names
    for p in SOURCE.rglob('*.mat'):
        manifest['source_hashes'][str(p)] = digest(p)
    for p in (SOURCE/'Model_DATA/Prefab').rglob('*.prefab'):
        manifest['source_hashes'][str(p)] = digest(p)
    catalog = []
    for p in (SOURCE/'Scenes/DemoScene_Data/ScriptableObject').rglob('*.asset'):
        text = p.read_text(encoding='utf-8-sig')
        mesh = field(text, '_meshName')
        if mesh:
            catalog.append(dict(source=str(p.relative_to(SOURCE)), slot=p.parent.name, mesh_pattern=mesh,
                                content_id=field(text,'_contentId'), display_name=field(text,'_displayName')))
    manifest['selection_catalog'] = catalog
    assert all(digest(Path(p)) == h for p,h in manifest['source_hashes'].items())
    (OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    print('P09 export complete:', len(manifest['parts']), 'parts', len(materials), 'materials', presets)


if __name__ == '__main__':
    main()
