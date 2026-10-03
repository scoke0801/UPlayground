"""Import Blender projectile art, or verify the rebuilt native default.

UnrealEditor-Cmd UPlayground.uproject -EnablePlugins=PythonScriptPlugin
  -run=pythonscript -script=<this file> -unattended -NullRHI
Add -PGValidateProjectile after building UPlaygroundEditor to check the binding.
"""
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / 'Tools/Art/Projectiles'
OUT = ROOT / 'Saved/ProjectileArt'
DEST = '/Game/Art/Projectiles'
OUT.mkdir(parents=True, exist_ok=True)
manifest = json.loads((SOURCE/'manifest.json').read_text(encoding='utf-8'))
MESH_PATH = DEST + '/' + manifest['mesh']
MATERIAL_PATH = DEST + '/' + manifest['material']
validate_only = '-PGValidateProjectile' in unreal.SystemLibrary.get_command_line()


def vector(value):
    return [round(value.x, 5), round(value.y, 5), round(value.z, 5)]


def projectile_state():
    cdo = unreal.get_default_object(unreal.PGPatternProjectile)
    component = cdo.get_editor_property('mesh_component')
    box = cdo.get_editor_property('projectile_collision_box')
    movement = cdo.get_editor_property('movement_component')
    mesh = component.get_editor_property('static_mesh')
    return {
        'mesh': mesh.get_path_name() if mesh else None,
        'scale': vector(component.get_editor_property('relative_scale3d')),
        'mesh_collision': str(component.get_collision_enabled()),
        'box_extent': vector(box.get_unscaled_box_extent()),
        'box_collision': str(box.get_collision_enabled()),
        'box_responses': box.get_editor_property('body_instance').export_text(),
        'initial_speed': movement.get_editor_property('initial_speed'),
        'max_speed': movement.get_editor_property('max_speed'),
        'gravity': movement.get_editor_property('projectile_gravity_scale'),
        'rotation_follows_velocity': movement.get_editor_property('rotation_follows_velocity'),
        'damage': cdo.get_editor_property('damage'),
        'life_time': cdo.get_editor_property('life_time'),
    }


before = projectile_state()
if not validate_only:
    backup = ROOT/'Saved/Backups/ProjectileArt'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    for path in (MESH_PATH, MATERIAL_PATH):
        relative = Path(path.removeprefix('/Game/')+'.uasset')
        source = ROOT/'Content'/relative
        if source.is_file():
            target = backup/relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    if not (OUT/'before.json').exists():
        (OUT/'before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = unreal.load_asset(MATERIAL_PATH) if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH) else None
    if not mat:
        mat = asset_tools.create_asset(manifest['material'], DEST, unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(mat)
    color = lib.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -500, 0)
    lib.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    mask = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -270, 160)
    lib.connect_material_expressions(color, 'RGB', mask, 'A')
    lib.connect_material_expressions(color, 'A', mask, 'B')
    glow = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -40, 160)
    glow.set_editor_property('const_b', manifest['emission'])
    lib.connect_material_expressions(mask, '', glow, 'A')
    lib.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    for key, prop, y in [('roughness', unreal.MaterialProperty.MP_ROUGHNESS, 350),
                          ('metallic', unreal.MaterialProperty.MP_METALLIC, 450)]:
        node = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, y)
        node.set_editor_property('r', manifest[key])
        lib.connect_material_property(node, '', prop)
    lib.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat)

    task = unreal.AssetImportTask()
    for key, value in [('filename', str(SOURCE/manifest['fbx'])), ('destination_path', DEST),
                       ('destination_name', manifest['mesh']), ('automated', True),
                       ('replace_existing', True), ('save', True), ('factory', unreal.FbxFactory())]:
        task.set_editor_property(key, value)
    options = unreal.FbxImportUI()
    for key, value in [('automated_import_should_detect_type', False),
                       ('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH),
                       ('import_mesh', True), ('import_materials', False),
                       ('import_textures', False), ('import_animations', False)]:
        options.set_editor_property(key, value)
    static = options.get_editor_property('static_mesh_import_data')
    for key, value in [('combine_meshes', True), ('auto_generate_collision', False),
                       ('generate_lightmap_u_vs', False), ('convert_scene', True),
                       ('convert_scene_unit', True), ('force_front_x_axis', False),
                       ('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE),
                       ('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)]:
        static.set_editor_property(key, value)
    task.set_editor_property('options', options)
    asset_tools.import_asset_tasks([task])
    mesh = unreal.load_asset(MESH_PATH)
    assert isinstance(mesh, unreal.StaticMesh), task.get_editor_property('imported_object_paths')
    mesh.set_material(0, mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)

mesh = unreal.load_asset(MESH_PATH)
assert isinstance(mesh, unreal.StaticMesh)
box = mesh.get_bounding_box()
bounds = [vector(box.min), vector(box.max)]
assert all(abs(bounds[side][axis]-manifest['bounds_m'][side][axis]*100) < .03
           for side in range(2) for axis in range(3)), ('FBX axis/units', bounds)
assert mesh.get_num_triangles(0) == manifest['triangles']
assert len(mesh.get_editor_property('static_materials')) == 1
assert mesh.get_material(0).get_path_name().split('.')[0] == MATERIAL_PATH
geometry = mesh.get_editor_property('body_setup').get_editor_property('agg_geom')
assert all(len(geometry.get_editor_property(field)) == 0
           for field in ('box_elems', 'sphere_elems', 'sphyl_elems', 'convex_elems'))
table = unreal.load_asset('/Game/DataCenter/DataTables/Skill/DT_Skill')
skills = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
skill = next(row for row in skills if row['SkillID'] == manifest['skill_id'])
assert unreal.load_class(None, skill['ProjectileClass']) == unreal.PGPatternProjectile.static_class()
after = projectile_state()
if validate_only:
    assert after['mesh'].split('.')[0] == MESH_PATH, after
    assert after['scale'] == [1, 1, 1], after
    assert (OUT/'before.json').exists(), 'Run import before validating the replacement'
    baseline = json.loads((OUT/'before.json').read_text(encoding='utf-8'))
    for key in baseline.keys() - {'mesh', 'scale'}:
        assert after[key] == baseline[key], ('Gameplay setting changed', key, baseline[key], after[key])
report = {'status': 'PASS', 'binding_verified': validate_only, 'mesh': MESH_PATH,
          'bounds_cm': bounds, 'triangles': mesh.get_num_triangles(0), 'material_slots': 1,
          'skill_id': manifest['skill_id'], 'projectile': after}
(OUT/('validation.json' if validate_only else 'import.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('PGProjectileArt PASS ' + json.dumps(report))
