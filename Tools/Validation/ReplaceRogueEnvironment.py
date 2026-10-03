"""Import the Blender kit; -PGApplyEnvironment also updates RogueArena.

Use UnrealEditor-Cmd -EnablePlugins=PythonScriptPlugin -run=pythonscript
-script=<this file> -NullRHI -unattended. Imports/validation happen before map
writes. The source map and all its external actors are backed up before applying.
"""
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / 'Tools/Art/RogueEnvironment'
DEST = '/Game/Environment/RogueArena'
MAP = '/Game/Maps/RogueArena'
OUT = ROOT / 'Saved/RogueEnvironment'
OUT.mkdir(parents=True, exist_ok=True)
manifest = json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8'))
apply = '-PGApplyEnvironment' in unreal.SystemLibrary.get_command_line()
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
materials = {}


def vector(v):
    return [round(v.x, 5), round(v.y, 5), round(v.z, 5)]


def collision_signature(mesh):
    body = mesh.get_editor_property('body_setup')
    agg = body.get_editor_property('agg_geom')
    return {
        'trace_policy': str(body.get_editor_property('collision_trace_flag')),
        # ExportText includes hull vertices that are not individually exposed
        # to Python as editable fields in UE 5.8.
        'geometry': agg.export_text(),
        'shape_count': sum(len(agg.get_editor_property(field)) for field in ['box_elems','convex_elems','sphere_elems','sphyl_elems']),
    }


def make_material(name, definition):
    color, roughness, metallic, emission = definition
    full_name = 'M_PGEnv_' + name
    path = DEST + '/Materials/' + full_name
    mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not mat:
        mat = asset_tools.create_asset(full_name, DEST + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
        lib = unreal.MaterialEditingLibrary
        base = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
        base.set_editor_property('constant', unreal.LinearColor(*color, 1))
        lib.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
        for prop, value, y in [(unreal.MaterialProperty.MP_ROUGHNESS, roughness, 180), (unreal.MaterialProperty.MP_METALLIC, metallic, 280)]:
            node = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, y)
            node.set_editor_property('r', value)
            lib.connect_material_property(node, '', prop)
        if emission:
            strength = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 380)
            strength.set_editor_property('const_b', emission)
            lib.connect_material_expressions(base, '', strength, 'A')
            lib.connect_material_property(strength, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        lib.recompile_material(mat)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


for name, definition in manifest['materials'].items():
    materials['M_PGEnv_' + name] = make_material(name, definition)

tasks = []
for name, data in manifest['meshes'].items():
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(SOURCE / data['file']))
    task.set_editor_property('destination_path', DEST + '/Meshes')
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    task.set_editor_property('factory', unreal.FbxFactory())
    options = unreal.FbxImportUI()
    options.set_editor_property('automated_import_should_detect_type', False)
    options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property('import_mesh', True)
    options.set_editor_property('import_materials', False)
    options.set_editor_property('import_textures', False)
    options.set_editor_property('import_animations', False)
    static = options.get_editor_property('static_mesh_import_data')
    for prop, value in [('combine_meshes', True), ('auto_generate_collision', False), ('generate_lightmap_u_vs', False), ('convert_scene', True), ('convert_scene_unit', True), ('force_front_x_axis', False), ('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)]:
        static.set_editor_property(prop, value)
    task.set_editor_property('options', options)
    tasks.append(task)
if '-PGUseImportedEnvironment' not in unreal.SystemLibrary.get_command_line():
    asset_tools.import_asset_tasks(tasks)

report = {'map': MAP, 'applied': False, 'meshes': {}, 'changes': []}
meshes = {}
for name, data in manifest['meshes'].items():
    mesh = unreal.load_asset(DEST + '/Meshes/' + name)
    assert isinstance(mesh, unreal.StaticMesh), name
    slots = mesh.get_editor_property('static_materials')
    slot_names = []
    for i, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property('imported_material_slot_name'))
        assert slot_name in materials, (name, slot_name)
        mesh.set_material(i, materials[slot_name])
        slot_names.append(slot_name)
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    if name in ['SM_PG_ArenaFloor', 'SM_PG_ArenaRampart']:
        # Reuse the tested prototype's exact primitive and trace policy. This
        # also avoids importer-dependent handling of authored UBX FBX nodes.
        prototype_body = unreal.load_asset('/Game/ExternalAssets/LevelDesign/LevelPrototyping/Meshes/SM_Cube').get_editor_property('body_setup')
        body.set_editor_property('agg_geom', prototype_body.get_editor_property('agg_geom'))
        body.set_editor_property('collision_trace_flag', prototype_body.get_editor_property('collision_trace_flag'))
    agg = body.get_editor_property('agg_geom') if body else None
    boxes = list(agg.get_editor_property('box_elems')) if agg else []
    collision_boxes = [{'center': vector(b.get_editor_property('center')), 'size': [b.get_editor_property(axis) for axis in ['x', 'y', 'z']]} for b in boxes]
    report['meshes'][name] = {'path': mesh.get_path_name(), 'bounds': [vector(box.min), vector(box.max)], 'triangles': mesh.get_num_triangles(0), 'slots': slot_names, 'box_collision': collision_boxes, 'convex_count': len(agg.get_editor_property('convex_elems')) if agg else 0, 'collision': collision_signature(mesh)}
    meshes[name] = mesh
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
(OUT / 'import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')

# Validate centimetres, pivots and exact prototype collision before touching the map.
for name, row in report['meshes'].items():
    expected = manifest['meshes'][name]['bounds_m']
    assert all(abs(row['bounds'][side][axis] - expected[side][axis] * 100) < .03 for side in range(2) for axis in range(3)), (name, 'FBX bounds differ', row['bounds'], expected)
    assert row['triangles'] > 0
prototype_collision = collision_signature(unreal.load_asset('/Game/ExternalAssets/LevelDesign/LevelPrototyping/Meshes/SM_Cube'))
assert prototype_collision['shape_count'] > 0
for name in ['SM_PG_ArenaFloor', 'SM_PG_ArenaRampart']:
    assert report['meshes'][name]['collision'] == prototype_collision, (name, 'Collision differs from prototype')

if apply:
    timestamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup = ROOT / 'Saved/Backups/RogueEnvironment' / timestamp
    for relative in ['Maps/RogueArena.umap', 'Maps/RogueArena_HLODLayer_Instanced.uasset', 'Maps/RogueArena_HLODLayer_Merged.uasset', '__ExternalActors__/Maps/RogueArena', '__ExternalObjects__/Maps/RogueArena']:
        source = ROOT / 'Content' / relative
        target = backup / relative
        if source.is_dir():
            shutil.copytree(source, target)
        elif source.is_file():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    report['backup'] = str(backup)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.load_level(MAP)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    original_count = len(actors.get_all_level_actors())
    walls = {'SM_Cube17', 'SM_Cube20', 'SM_Cube21', 'SM_Cube22', 'SM_Cube23'}
    expected_labels = walls | {'SM_Cube15'} | {f'RogueDecor_{kind}{i}' for kind in ['Plinth','Crystal','Band'] for i in range(8)}
    by_label = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
    assert expected_labels <= by_label.keys(), expected_labels - by_label.keys()
    prototype = '/Game/ExternalAssets/LevelDesign/LevelPrototyping/Meshes/SM_Cube.SM_Cube'
    for label in sorted(expected_labels):
        actor = by_label[label]
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        old_mesh = comp.get_editor_property('static_mesh')
        old_transform = actor.get_actor_transform()
        row = {'actor': label, 'previous_mesh': old_mesh.get_path_name(), 'location_before': vector(actor.get_actor_location()), 'scale_before': vector(actor.get_actor_scale3d()), 'collision_before': str(comp.get_collision_enabled())}
        if label == 'SM_Cube15': name='SM_PG_ArenaFloor'
        elif label in walls: name='SM_PG_ArenaRampart'
        elif 'Plinth' in label: name='SM_PG_CrystalPlinth'
        elif 'Crystal' in label: name='SM_PG_CrystalCluster'
        else: name='SM_PG_RuneSeal'
        assert old_mesh.get_path_name() == prototype or old_mesh.get_path_name().startswith(('/Engine/BasicShapes/', DEST)), (label, old_mesh.get_path_name())
        actor.modify(); comp.modify()
        comp.set_static_mesh(meshes[name])
        comp.set_editor_property('override_materials', [])
        if label.startswith('RogueDecor_'):
            # The original arena builder intended these to be decorative only.
            comp.set_collision_profile_name('NoCollision')
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            comp.set_editor_property('cast_shadow', True)
            if 'Band' in label:
                actor.set_actor_scale3d(unreal.Vector(1,1,1))
                actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=int(label[-1])*45,roll=0),False)
            if int(label[-1]) % 2:
                for i, material_name in enumerate(report['meshes'][name]['slots']):
                    purple_name=material_name.replace('Mint','Violet')
                    if purple_name != material_name: comp.set_material(i,materials[purple_name])
        else:
            assert str(actor.get_actor_transform()) == str(old_transform) or vector(actor.get_actor_location()) == row['location_before']
            assert comp.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
        # SM_Cube20 and SM_Cube21 overlap by 29.99m. Retain both collision actors,
        # but draw only one wall to remove overlapping masonry and z-fighting.
        if label == 'SM_Cube20':
            comp.set_visibility(False,True)
            comp.set_hidden_in_game(True,True)
            comp.set_editor_property('cast_shadow',False)
        row.update(mesh=meshes[name].get_path_name(), location_after=vector(actor.get_actor_location()), scale_after=vector(actor.get_actor_scale3d()), collision_after=str(comp.get_collision_enabled()))
        report['changes'].append(row)
    assert len(report['changes']) == 30
    assert len(actors.get_all_level_actors()) == original_count
    assert level.save_current_level()
    report['applied'] = True
    (OUT / 'replacement.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('PG_ENVIRONMENT_COMPLETE applied=' + str(report['applied']))
