"""Author an original terrain surface and gameplay collision in a fresh Blender CLI.

The environment props are imported source resources. This terrain is new geometry,
not a copied Unity terrain or demo layout. Dimensions below are in metres.
"""
import bpy
import json
import math
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
OUT=Path(__file__).resolve().parent
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.name='PG_ForestRuins_Terrain'
scene.unit_settings.system='METRIC'
scene.unit_settings.scale_length=1.

def mesh_object(name,vertices,faces):
    mesh=bpy.data.meshes.new(name)
    mesh.from_pydata(vertices,[],faces)
    mesh.update()
    obj=bpy.data.objects.new(name,mesh)
    scene.collection.objects.link(obj)
    return obj

def box(vertices,faces,center,size):
    start=len(vertices)
    for dx,dy,dz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]:
        vertices.append(tuple(center[i]+v*size[i]/2 for i,v in enumerate((dx,dy,dz))))
    faces.extend(tuple(start+i for i in face) for face in [(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)])

# A continuous flat gameplay surface; gentle terrain relief starts outside walls.
size=56
vertices=[]
for row in range(size+1):
    for col in range(size+1):
        x,y=-56+col*2,-56+row*2
        # A rectangular plateau keeps all four playable corners at collision Z=0.
        outside=max(0.,abs(x)/25.-1.,abs(y)/21.-1.)
        z=0 if outside==0 else min(2.4,outside*.8)*(0.6+0.4*math.sin(x*.14)*math.cos(y*.19))
        vertices.append((x,y,z))
faces=[]
for row in range(size):
    for col in range(size):
        a=row*(size+1)+col
        faces.extend([(a,a+1,a+size+2),(a,a+size+2,a+size+1)])
ground=mesh_object('SM_PGFR_Ground',vertices,faces)
uv=ground.data.uv_layers.new(name='UVMap')
colors=ground.data.color_attributes.new(name='Color',type='BYTE_COLOR',domain='CORNER')
for poly in ground.data.polygons:
    for loop in poly.loop_indices:
        v=ground.data.vertices[ground.data.loops[loop].vertex_index].co
        uv.data[loop].uv=(v.x/4,v.y/4)
        d=math.sqrt((v.x/23)**2+(v.y/19)**2)
        grass=max(0,min(1,(d-.76)/.40))
        # Subtle worn cross paths; preserve quiet dirt beneath combat effects.
        trail=min(abs(v.x+2.5),abs(v.y-1.2))
        if d<1.12: grass*=max(.20,min(1,(trail-1.3)/2))
        colors.data[loop].color=(grass,0,0,1)
ground.data.materials.append(bpy.data.materials.new('M_PGFR_Ground'))

# Solid collision under the fighting area, top Z=0. Top remains continuous.
v,f=[],[]
box(v,f,(0,0,-.3),(48,40,.6))
floor=mesh_object('UBX_SM_PGFR_Ground_00',v,f)

# Boundary collision follows the visible perimeter stone course.
v,f=[],[]
for center,extent in [((23.5,0,.45),(.8,40,.9)),((-23.5,0,.45),(.8,40,.9)),
                      ((0,19.5,.45),(48,.8,.9)),((0,-19.5,.45),(48,.8,.9))]: box(v,f,center,extent)
boundary=mesh_object('SM_PGFR_Boundary',v,f)
colliders=[]
for i,(center,extent) in enumerate([((23.5,0,.45),(.8,40,.9)),((-23.5,0,.45),(.8,40,.9)),
                                  ((0,19.5,.45),(48,.8,.9)),((0,-19.5,.45),(48,.8,.9))]):
    cv,cf=[],[];box(cv,cf,center,extent)
    colliders.append(mesh_object('UBX_SM_PGFR_Boundary_'+str(i).zfill(2),cv,cf))

for obj,collision in [(ground,[floor]),(boundary,colliders)]:
    bpy.ops.object.select_all(action='DESELECT')
    for item in [obj]+collision: item.select_set(True)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.fbx(filepath=str(OUT/(obj.name+'.fbx')),use_selection=True,
        object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,
        add_leaf_bones=False,use_mesh_modifiers=True,path_mode='AUTO')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'PG_ForestRuins_Terrain.blend'))
(OUT/'terrain.json').write_text(json.dumps(dict(schema=1,playable_cm=[[-2310,-1910,0],[2310,1910,0]],
    terrain_size_cm=[11200,11200],authored_here=True,flat_combat_surface=True),indent=2),encoding='utf-8')
print('PG_FOREST_GROUND COMPLETE')
