"""Build Blender-authored utility meshes used by development maps and loot VFX.

Run with Blender in background mode. The generated FBX files intentionally
preserve the bounds and pivots of the Unreal primitive meshes they replace.
"""

import json
import math
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector


OUT = Path(__file__).resolve().parent
OUT.mkdir(parents=True, exist_ok=True)

scene = bpy.data.scenes.new("PG_UtilityModels")
bpy.context.window.scene = scene
scene.unit_settings.system = "METRIC"
scene.unit_settings.scale_length = 1.0
collection = bpy.data.collections.new("PG_UtilityModelSources")
scene.collection.children.link(collection)


def source_material(name, color, metallic, roughness, emission=0.0):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    shader = material.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1.0)
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = roughness
    if emission:
        shader.inputs["Emission Color"].default_value = (*color, 1.0)
        shader.inputs["Emission Strength"].default_value = emission
    return material


dev_material = source_material("M_PG_DevBlock", (0.10, 0.15, 0.21), 0.32, 0.64)
beam_material = source_material("M_PG_LootBeam", (0.35, 0.82, 0.72), 0.05, 0.28, 1.8)


def add_uv(mesh):
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        dominant = max(range(3), key=lambda axis: abs(polygon.normal[axis]))
        axes = [axis for axis in range(3) if axis != dominant]
        for loop_index in polygon.loop_indices:
            coordinate = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            uv_layer.data[loop_index].uv = (coordinate[axes[0]], coordinate[axes[1]])


def make_block(name, center):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.translate(bm, verts=list(bm.verts), vec=center)
    bmesh.ops.bevel(
        bm,
        geom=list(bm.edges),
        offset=0.025,
        segments=2,
        affect="EDGES",
        clamp_overlap=True,
    )
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.append(dev_material)
    mesh.update()
    add_uv(mesh)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    return obj


def make_loot_beam():
    name = "SM_PG_LootBeam"
    sides = 12
    rings = [(-0.5, 0.34), (-0.43, 0.5), (0.43, 0.5), (0.5, 0.34)]
    vertices = [
        (radius * math.cos(index * math.tau / sides),
         radius * math.sin(index * math.tau / sides), z)
        for z, radius in rings
        for index in range(sides)
    ]
    faces = [tuple(reversed(range(sides))), tuple(range((len(rings) - 1) * sides, len(rings) * sides))]
    for ring in range(len(rings) - 1):
        for index in range(sides):
            faces.append((
                ring * sides + index,
                ring * sides + (index + 1) % sides,
                (ring + 1) * sides + (index + 1) % sides,
                (ring + 1) * sides + index,
            ))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(beam_material)
    mesh.update()
    add_uv(mesh)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    return obj


objects = {
    # FBX's Blender-to-Unreal conversion negates Blender Y. Author the corner
    # pivot in negative Y so the imported Unreal bounds remain 0..100 cm.
    "SM_PG_DevBlock_Corner": make_block("SM_PG_DevBlock_Corner", (0.5, -0.5, 0.5)),
    "SM_PG_DevBlock_Centered": make_block("SM_PG_DevBlock_Centered", (0.0, 0.0, 0.0)),
    "SM_PG_LootBeam": make_loot_beam(),
}


def export_object(name, obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    result = bpy.ops.export_scene.fbx(
        filepath=str(OUT / f"{name}.fbx"),
        use_selection=True,
        object_types={"MESH"},
        bake_anim=False,
        add_leaf_bones=False,
        axis_forward="-Y",
        axis_up="Z",
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_NONE",
        mesh_smooth_type="FACE",
        use_triangles=True,
    )
    assert "FINISHED" in result, (name, result)
    obj.data.calc_loop_triangles()
    bounds = [list(vertex) for vertex in obj.bound_box]
    return {
        "file": f"{name}.fbx",
        "bounds_m": [list(map(min, zip(*bounds))), list(map(max, zip(*bounds)))],
        "triangles": len(obj.data.loop_triangles),
        "vertices": len(obj.data.vertices),
        "material": obj.data.materials[0].name,
    }


mesh_manifest = {name: export_object(name, obj) for name, obj in objects.items()}
# Store post-import Unreal bounds. Blender Y is negated by the FBX axis
# conversion for the corner-pivot block.
mesh_manifest["SM_PG_DevBlock_Corner"]["bounds_m"] = [[0.0, 0.0, 0.0], [1.0, 1.0, 1.0]]

manifest = {
    "schema": 1,
    "blender_file": "PG_UtilityModels.blend",
    "unit": "metres",
    "destination": "/Game/Art/UtilityModels",
    "meshes": mesh_manifest,
    "replacements": {
        "/Game/ExternalAssets/LevelDesign/LevelPrototyping/Meshes/SM_Cube.SM_Cube": "SM_PG_DevBlock_Corner",
        "/Engine/BasicShapes/Cube.Cube": "SM_PG_DevBlock_Centered",
        "/Engine/BasicShapes/Cylinder.Cylinder": "SM_PG_LootBeam",
    },
    "materials": {
        "M_PG_DevBlock": {"color": [0.10, 0.15, 0.21], "metallic": 0.32, "roughness": 0.64, "emission": 0.0},
        "M_PG_LootBeam": {"color": [0.35, 0.82, 0.72], "metallic": 0.05, "roughness": 0.28, "emission": 1.8},
    },
}

(OUT / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

# Keep export sources at their authored origins and use linked copies for a
# human-readable Blender preview.
preview_collection = bpy.data.collections.new("PG_UtilityModelPreview")
scene.collection.children.link(preview_collection)
for source, location, scale in [
    (objects["SM_PG_DevBlock_Corner"], (-1.45, 0.0, 0.0), (1.0, 1.0, 1.0)),
    (objects["SM_PG_DevBlock_Centered"], (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
    (objects["SM_PG_LootBeam"], (1.45, 0.0, 0.0), (0.55, 0.55, 1.35)),
]:
    source.hide_render = True
    preview = source.copy()
    preview.data = source.data
    preview.name = "Preview_" + source.name
    preview.hide_render = False
    preview.location = location
    preview.scale = scale
    preview_collection.objects.link(preview)

camera_data = bpy.data.cameras.new("PG_UtilityCamera")
camera = bpy.data.objects.new("PG_UtilityCamera", camera_data)
preview_collection.objects.link(camera)
camera.location = (4.2, -6.2, 3.8)
camera.rotation_euler = (Vector((0.0, 0.0, 0.45)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera_data.type = "ORTHO"
camera_data.ortho_scale = 5.0
scene.camera = camera

for name, location, energy, size in [
    ("Key", (-2.5, -3.0, 5.0), 850.0, 4.0),
    ("Rim", (3.5, 1.5, 3.0), 600.0, 3.0),
]:
    light_data = bpy.data.lights.new("PG_Utility" + name, "AREA")
    light_data.energy = energy
    light_data.shape = "DISK"
    light_data.size = size
    light = bpy.data.objects.new(light_data.name, light_data)
    preview_collection.objects.link(light)
    light.location = location
    light.rotation_euler = (Vector((0.0, 0.0, 0.3)) - light.location).to_track_quat("-Z", "Y").to_euler()

world = bpy.data.worlds.new("PG_UtilityWorld")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.012, 0.02, 0.035, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.35
scene.world = world
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = 1400
scene.render.resolution_y = 700
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(OUT / "Preview_Blender.png")
scene.view_settings.look = "AgX - Medium High Contrast"
bpy.ops.render.render(write_still=True)

bpy.data.libraries.write(str(OUT / manifest["blender_file"]), {scene}, fake_user=True)
print("PG utility models built:", json.dumps(manifest, indent=2))
