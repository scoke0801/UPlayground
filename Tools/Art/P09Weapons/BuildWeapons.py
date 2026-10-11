"""Blender: original low-poly polearms and long-grip greatswords, centimetre units.

The palm is at the origin; Blender +Y exports to Unreal -Y, matching P09 weapons.
No source character, animation or weapon package is modified.
"""
import bpy
import math
from pathlib import Path

OUT = Path(__file__).resolve().parent
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = .01


def material(name, color, metallic, roughness):
    m = bpy.data.materials.new(name); m.diffuse_color = (*color, 1)
    m.use_nodes = True
    p = m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = (*color,1)
    p.inputs['Metallic'].default_value = metallic
    p.inputs['Roughness'].default_value = roughness
    return m


steel = material('Steel', (.25,.34,.43), .8, .3)
edge = material('Edge', (.63,.72,.78), .85, .22)
grip = material('Grip', (.09,.035,.018), .05, .72)
gold = material('Gold', (.52,.30,.075), .75, .32)


def cylinder(y, length, radius, mat, vertices=12):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=length, location=(0,y,0), rotation=(math.pi/2,0,0))
    obj = bpy.context.object; obj.data.materials.append(mat)
    return obj


def blade(start, length, width, thickness):
    # A bevelled diamond cross-section and four-sided tip give readable steel facets.
    verts = [(0,start,thickness), (width,start,0), (0,start,-thickness), (-width,start,0),
             (0,start+length*.67,thickness*.7),(width*.65,start+length*.67,0),
             (0,start+length*.67,-thickness*.7),(-width*.65,start+length*.67,0),(0,start+length,0)]
    faces=[(0,3,2,1)] + [(i,(i+1)%4,(i+1)%4+4,i+4) for i in range(4)] + [(i+4,(i+1)%4+4,8) for i in range(4)]
    mesh=bpy.data.meshes.new('ForgedBlade'); mesh.from_pydata(verts,[],faces);mesh.update()
    obj=bpy.data.objects.new('ForgedBlade',mesh);bpy.context.collection.objects.link(obj)
    mesh.materials.append(steel);mesh.materials.append(edge)
    for p in mesh.polygons:p.material_index=p.index%2
    return obj


for family in ('Spear','Greatsword'):
    for tier in range(3):
        bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
        if family=='Spear':
            cylinder(25,190,1.65,grip)
            cylinder(113,12,2.3,gold if tier==2 else steel)
            cylinder(-68,10,2.,steel)
            blade(115,38+tier*5,5+tier,1.4)
            for y in (-10,10,93,103):cylinder(y,2.,2.,gold if tier else steel)
            if tier>0:
                bpy.ops.mesh.primitive_cube_add(size=1,location=(0,115,0));bpy.context.object.dimensions=(22+tier*4,2.8,3.)
                bpy.context.object.data.materials.append(gold if tier==2 else steel)
        else:
            cylinder(0,34,2.1,grip)
            cylinder(-19,6,3.8,gold if tier==2 else steel)
            bpy.ops.mesh.primitive_cube_add(size=1,location=(0,19,0));bpy.context.object.dimensions=(29+tier*3,4,4)
            bpy.context.object.data.materials.append(gold if tier==2 else steel)
            blade(21,125+tier*8,7+tier,2.)
            for y in (-13,-6,1,8,14):cylinder(y,1.,2.35,gold if tier==2 else steel)
        bpy.ops.object.select_all(action='SELECT')
        bpy.context.view_layer.objects.active=next(o for o in bpy.context.scene.objects if o.type=='MESH')
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        bpy.ops.object.join()
        obj=bpy.context.object;obj.name=f'SM_PG_{family}_{tier}'
        bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
        bpy.ops.export_scene.fbx(filepath=str(OUT/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},
            axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False)
