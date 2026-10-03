"""Build the temporary +X-forward crystal bolt in Blender (metres).

Run through Blender MCP or: blender --background --python <this file>.
Creates a separate scene; existing open scenes and objects are preserved.
"""
import json
import math
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

OUT = Path(__file__).resolve().parent
OUT.mkdir(parents=True, exist_ok=True)
scene = bpy.data.scenes.new('PG_ProjectileArt')
bpy.context.window.scene = scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
collection = bpy.data.collections.new('PG_ProjectileSources')
scene.collection.children.link(collection)

# RGB is the surface colour; alpha is the emission mask. One material/section.
PALETTE = {
    'Core': (1.0, .42, .065, .55),
    'Light': (1.0, .80, .32, .85),
    'Shade': (.56, .095, .018, .20),
    'Metal': (.16, .105, .055, .02),
    'Gold': (.58, .30, .065, .07),
}
mat = bpy.data.materials.new('M_PG_CrystalBolt')
mat.use_nodes = True
shader = mat.node_tree.nodes.get('Principled BSDF')
color = mat.node_tree.nodes.new('ShaderNodeVertexColor')
color.layer_name = 'Col'
strength = mat.node_tree.nodes.new('ShaderNodeMath')
strength.operation = 'MULTIPLY'
strength.inputs[1].default_value = 1.4
links = mat.node_tree.links
links.new(color.outputs['Color'], shader.inputs['Base Color'])
links.new(color.outputs['Color'], shader.inputs['Emission Color'])
links.new(color.outputs['Alpha'], strength.inputs[0])
links.new(strength.outputs[0], shader.inputs['Emission Strength'])
shader.inputs['Roughness'].default_value = .38
shader.inputs['Metallic'].default_value = .25
vertices, faces, colors = [], [], []


def add(verts, polys, palette):
    offset = len(vertices)
    vertices.extend(verts)
    faces.extend(tuple(offset + index for index in face) for face in polys)
    colors.extend([PALETTE[palette]] * len(polys))


def spindle(rings, palette, sides=8):
    verts = [(x, radius * math.cos(i * math.tau / sides),
              radius * math.sin(i * math.tau / sides))
             for x, radius in rings for i in range(sides)]
    add(verts, [tuple(reversed(range(sides))),
                tuple(range((len(rings)-1)*sides, len(rings)*sides))], palette)
    for row in range(len(rings)-1):
        for i in range(sides):
            shade = ('Light' if i in (1, 2) else 'Shade' if i in (5, 6) else palette) if palette == 'Core' else palette
            add(verts, [(row*sides+i, row*sides+(i+1)%sides,
                         (row+1)*sides+(i+1)%sides, (row+1)*sides+i)], shade)


# The bright spear point and a darker rear collar make travel direction clear.
spindle([(-.155, .025), (-.07, .072), (.035, .078), (.105, .055), (.22, .0008)], 'Core')
spindle([(-.145, .038), (-.135, .060), (-.105, .065), (-.09, .052)], 'Metal')
spindle([(-.139, .048), (-.134, .062), (-.125, .064), (-.120, .054)], 'Gold')
spindle([(-.109, .062), (-.103, .066), (-.095, .058)], 'Gold')

# Four solid swept fins, not transparent cards, remain readable from above.
for angle in (0, math.pi/2, math.pi, 3*math.pi/2):
    outline = [(-.22, .100), (-.185, .11), (-.07, .065), (-.085, .038), (-.17, .045)]
    verts = []
    for thickness in (-.005, .005):
        for x, radius in outline:
            verts.append((x, radius*math.cos(angle)-thickness*math.sin(angle),
                          radius*math.sin(angle)+thickness*math.cos(angle)))
    n = len(outline)
    add(verts, [tuple(reversed(range(n))), tuple(range(n, n*2))], 'Gold')
    add(verts, [(i, (i+1)%n, (i+1)%n+n, i+n) for i in range(n)], 'Light')

mesh = bpy.data.meshes.new('SM_PG_CrystalBolt')
mesh.from_pydata(vertices, [], faces)
mesh.materials.append(mat)
attribute = mesh.color_attributes.new(name='Col', type='FLOAT_COLOR', domain='CORNER')
for poly, rgba in zip(mesh.polygons, colors):
    for loop in poly.loop_indices:
        attribute.data[loop].color = rgba
bm = bmesh.new()
bm.from_mesh(mesh)
bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.00000001)
bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
bm.to_mesh(mesh)
bm.free()
mesh.update()
uv = mesh.uv_layers.new(name='UVMap')
for poly in mesh.polygons:
    dominant = max(range(3), key=lambda axis: abs(poly.normal[axis]))
    axes = [axis for axis in range(3) if axis != dominant]
    for index in poly.loop_indices:
        co = mesh.vertices[mesh.loops[index].vertex_index].co
        uv.data[index].uv = (co[axes[0]]*2+.5, co[axes[1]]*2+.5)
obj = bpy.data.objects.new('SM_PG_CrystalBolt', mesh)
collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
bpy.context.view_layer.update()
assert 'FINISHED' in bpy.ops.export_scene.fbx(
    filepath=str(OUT/'SM_PG_CrystalBolt.fbx'), use_selection=True,
    object_types={'MESH'}, bake_anim=False, add_leaf_bones=False,
    axis_forward='-Y', axis_up='Z', apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='FACE', use_triangles=True)
mesh.calc_loop_triangles()
bounds = [list(v) for v in obj.bound_box]
manifest = {
    'schema': 1, 'mesh': 'SM_PG_CrystalBolt', 'material': 'M_PG_CrystalBolt',
    'fbx': 'SM_PG_CrystalBolt.fbx', 'forward': '+X', 'unit': 'metres',
    'bounds_m': [list(map(min, zip(*bounds))), list(map(max, zip(*bounds)))],
    'triangles': len(mesh.loop_triangles), 'vertices': len(mesh.vertices),
    'material_slots': 1, 'vertex_color': 'RGB=base colour, A=emission mask',
    'roughness': .38, 'metallic': .25, 'emission': 1.4,
    'runtime_class': '/Script/PGActor.PGPatternProjectile', 'skill_id': 15102,
}
(OUT/'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')

camera_data = bpy.data.cameras.new('PG_ProjectileCamera')
camera = bpy.data.objects.new('PG_ProjectileCamera', camera_data)
collection.objects.link(camera)
camera.location = (.40, -.72, .55)
camera.rotation_euler = (Vector((0, 0, 0))-camera.location).to_track_quat('-Z', 'Y').to_euler()
camera_data.type = 'ORTHO'
camera_data.ortho_scale = .68
scene.camera = camera
for name, location, energy, size in [
    ('Key', (.1, -.4, .8), 35, .65), ('Rim', (-.1, .5, .3), 22, .40)]:
    light_data = bpy.data.lights.new('PG_Projectile'+name, 'AREA')
    light_data.energy, light_data.shape, light_data.size = energy, 'DISK', size
    light = bpy.data.objects.new(light_data.name, light_data)
    collection.objects.link(light)
    light.location = location
    light.rotation_euler = (-light.location).to_track_quat('-Z', 'Y').to_euler()
world = bpy.data.worlds.new('PG_ProjectileWorld')
world.use_nodes = True
world.node_tree.nodes['Background'].inputs[0].default_value = (.025, .035, .065, 1)
world.node_tree.nodes['Background'].inputs[1].default_value = .5
scene.world = world
scene.render.engine = 'CYCLES'
scene.cycles.samples = 48
scene.cycles.use_denoising = True
scene.render.resolution_x, scene.render.resolution_y = 1200, 800
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = str(OUT/'Preview_Blender.png')
scene.view_settings.view_transform = 'AgX'
# Only the new scene and its dependencies are written to this source file.
bpy.data.libraries.write(str(OUT/'PG_Projectiles.blend'), {scene}, fake_user=True)
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.region_3d.view_perspective = 'CAMERA'
result = manifest
