"""Run in Blender (MCP or --python). Author/export the RogueArena environment kit.

All dimensions are authored in metres, then normalized to the existing UE actor
scales. A separate scene is created; the user's existing scene is never cleared.
"""
import bpy
import bmesh
import json
import math
import random
from pathlib import Path
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Tools/Art/RogueEnvironment'
OUT.mkdir(parents=True, exist_ok=True)
assert bpy.context.mode == 'OBJECT', 'Switch to Object mode before building'
assert 'PG_RogueEnvironment' not in bpy.data.scenes, 'Kit scene already exists; use a fresh Blender session to rebuild'
scene = bpy.data.scenes.new('PG_RogueEnvironment')
bpy.context.window.scene = scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
sources = bpy.data.collections.new('PG_EnvironmentSources')
preview = bpy.data.collections.new('PG_EnvironmentPreview')
scene.collection.children.link(sources)
scene.collection.children.link(preview)
rng = random.Random(73109)

# Linear colors, shared with the Unreal import script.
PALETTE = {
    'Slate': ([.105, .135, .175], .88, .02, 0),
    'SlateLight': ([.145, .173, .205], .85, .02, 0),
    'SlateDark': ([.078, .098, .125], .91, .02, 0),
    'Basalt': ([.055, .069, .090], .86, .02, 0),
    'EdgeStone': ([.19, .215, .24], .77, .03, 0),
    'Bronze': ([.23, .145, .068], .46, .72, 0),
    'Inlay': ([.12, .14, .15], .69, .32, 0),
    'Mint': ([.035, .34, .24], .23, .18, .55),
    'MintLight': ([.16, .64, .49], .19, .08, .9),
    'Violet': ([.20, .075, .39], .23, .18, .55),
    'VioletLight': ([.43, .22, .72], .19, .08, .9),
}
materials = {}
for name, (color, roughness, metal, emission) in PALETTE.items():
    mat = bpy.data.materials.new('M_PGEnv_' + name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    p = mat.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = (*color, 1)
    p.inputs['Roughness'].default_value = roughness
    p.inputs['Metallic'].default_value = metal
    p.inputs['Emission Color'].default_value = (*color, 1)
    p.inputs['Emission Strength'].default_value = emission
    materials[name] = mat


class MeshBuilder:
    def __init__(self):
        self.vertices, self.faces, self.slots = [], [], []
        self.material_names = []

    def add(self, vertices, faces, material):
        offset = len(self.vertices)
        if material not in self.material_names:
            self.material_names.append(material)
        slot = self.material_names.index(material)
        self.vertices.extend(vertices)
        self.faces.extend(tuple(offset + i for i in face) for face in faces)
        self.slots.extend([slot] * len(faces))

    def box(self, center, size, bevel, material, yaw=0):
        x, y, z = [s * .5 for s in size]
        b = min(bevel, x * .3, y * .3, z * .3)
        vertices = []
        for inset, height in [(b, -z), (0, -z+b), (0, z-b), (b, z)]:
            a, c = x-inset, y-inset
            cut = max(b * .65, .00001)
            ring = [(-a+cut,-c),(a-cut,-c),(a,-c+cut),(a,c-cut),(a-cut,c),(-a+cut,c),(-a,c-cut),(-a,-c+cut)]
            for px, py in ring:
                vertices.append((center[0]+px*math.cos(yaw)-py*math.sin(yaw), center[1]+px*math.sin(yaw)+py*math.cos(yaw), center[2]+height))
        faces = [tuple(reversed(range(8))), tuple(range(24,32))]
        for j in range(3):
            faces.extend((j*8+i,j*8+(i+1)%8,(j+1)*8+(i+1)%8,(j+1)*8+i) for i in range(8))
        self.add(vertices, faces, material)

    def lathe(self, rings, material, sides=12, center=(0,0), phase=math.pi/12):
        verts = [(center[0]+radius*math.cos(phase+2*math.pi*i/sides), center[1]+radius*math.sin(phase+2*math.pi*i/sides), z) for z, radius in rings for i in range(sides)]
        faces = [tuple(reversed(range(sides))), tuple(range((len(rings)-1)*sides,len(rings)*sides))]
        for j in range(len(rings)-1):
            faces.extend((j*sides+i,j*sides+(i+1)%sides,(j+1)*sides+(i+1)%sides,(j+1)*sides+i) for i in range(sides))
        self.add(verts, faces, material)

    def ring(self, center, radius, width, z, material, segments=128, start=0, end=math.tau):
        verts = [(center[0]+r*math.cos(start+(end-start)*i/segments), center[1]+r*math.sin(start+(end-start)*i/segments), z) for i in range(segments+1) for r in [radius-width/2,radius+width/2]]
        self.add(verts, [(2*i,2*i+1,2*i+3,2*i+2) for i in range(segments)], material)

    def crystal(self, center, radius, height, yaw, lean):
        n=6
        verts=[]
        for z,r,dx in [(0,radius*.60,0),(height*.18,radius,0),(height*.68,radius*.78,lean*.6)]:
            verts += [(center[0]+dx+r*math.cos(yaw+i*math.tau/n), center[1]+r*math.sin(yaw+i*math.tau/n),center[2]+z) for i in range(n)]
        verts.append((center[0]+lean,center[1]+radius*.12,center[2]+height))
        self.add(verts,[tuple(reversed(range(n)))],'Mint')
        for i in range(n):
            mat='MintLight' if i in (1,4) else 'Mint'
            self.add(verts,[(i,(i+1)%n,n+(i+1)%n,n+i),(n+i,n+(i+1)%n,2*n+(i+1)%n,2*n+i),(2*n+i,2*n+(i+1)%n,3*n)],mat)

    def finish(self, name, normalization):
        mesh=bpy.data.meshes.new(name)
        mesh.from_pydata([tuple(v[i]/normalization[i] for i in range(3)) for v in self.vertices], [], self.faces)
        for material in self.material_names: mesh.materials.append(materials[material])
        for face,slot in zip(mesh.polygons,self.slots): face.material_index=slot
        bm=bmesh.new(); bm.from_mesh(mesh)
        bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.0000001)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        bm.to_mesh(mesh); bm.free(); mesh.update()
        obj=bpy.data.objects.new(name,mesh); sources.objects.link(obj)
        # UV0 uses planar metre coordinates. No baked lighting is used; these
        # deliberately tile rather than packing tiny bevels into an atlas.
        uv=mesh.uv_layers.new(name='UVMap')
        for poly in mesh.polygons:
            dominant=max(range(3),key=lambda i:abs(poly.normal[i]))
            axes=[i for i in range(3) if i!=dominant]
            for loop in poly.loop_indices:
                v=mesh.vertices[mesh.loops[loop].vertex_index].co
                uv.data[loop].uv=tuple(v[i]*normalization[i] for i in axes)
        return obj


# Continuous slab retains its exact walkable top at Z=.4 before actor scaling.
floor=MeshBuilder()
floor.box((15,15,.17),(30,30,.34),.012,'Basalt')
for row in range(24):
    for col in range(24):
        floor.box((.625+col*1.25,.625+row*1.25,.364),(1.236,1.236,.072),.009,rng.choices(['Slate','SlateLight','SlateDark'],[7,2,1])[0])
# A restrained perimeter course and ritual seal, below combat telegraph contrast.
for pos,size in [((15,1.10,.4005),(27.8,.026,.001)),((15,28.9,.4005),(27.8,.026,.001)),((1.10,15,.4005),(.026,27.8,.001)),((28.9,15,.4005),(.026,27.8,.001))]:
    floor.box(pos,size,.0001,'Bronze')
for radius,width in [(4.20,.055),(4.48,.027),(4.58,.045)]: floor.ring((15,15),radius,width,.401,'Inlay')
for i in range(32):
    angle=i*math.tau/32
    floor.box((15+4.34*math.cos(angle),15+4.34*math.sin(angle),.4015),(.11,.028,.001),.0001,'Inlay',angle)
for i in range(4):
    angle=i*math.pi/2
    floor.box((15+3.96*math.cos(angle),15+3.96*math.sin(angle),.4015),(.26,.26,.001),.0001,'Inlay',angle+math.pi/4)
floor_obj=floor.finish('SM_PG_ArenaFloor',(30,30,.4))

# Full-length wall panel, with masonry courses, buttresses, capstones and niches.
wall=MeshBuilder()
wall.box((15,.50,1.92),(30,.72,3.84),.02,'Basalt')
for face_y in [.075,.925]:
    for row in range(5):
        for col in range(20):
            width=1.46
            x=.75+col*1.5
            wall.box((x,face_y,.52+row*.58),(width,.145,.548),.018,rng.choice(['SlateDark','Basalt','Slate']))
for i in range(11):
    x=min(29.7,max(.30,i*3))
    wall.box((x,.5,.20),(.59,1,.40),.035,'EdgeStone')
    wall.box((x,.5,1.91),(.43,.94,3.04),.025,'Basalt')
    wall.box((x,.5,3.54),(.60,1,.23),.025,'EdgeStone')
    wall.box((x,.5,3.84),(.60,1,.32),.035,'EdgeStone')
    for face_y in [.021,.979]:
        wall.box((x,face_y,2.08),(.20,.036,1.42),.008,'Bronze')
        wall.box((x,face_y*.98+.01,2.10),(.12,.042,1.12),.006,'Basalt')
        wall.box((x,face_y,2.48),(.055,.045,.31),.004,'Mint')
for j in range(20):
    wall.box((j*1.5+.75,.5,3.77),(1.475,.98,.44),.028,'EdgeStone')
    wall.box((j*1.5+.75,.5,.13),(1.48,.98,.25),.02,'SlateDark')
for y in [.015,.985]: wall.box((15,y,3.42),(29.95,.026,.042),.005,'Bronze')
wall_obj=wall.finish('SM_PG_ArenaRampart',(30,1,4))

plinth=MeshBuilder()
plinth.lathe([(-.25,.78),(-.22,.90),(-.13,.90),(-.10,.81)],'SlateDark')
plinth.lathe([(-.10,.79),(-.07,.82),(.10,.72),(.14,.78),(.19,.78),(.22,.72)],'EdgeStone')
plinth.lathe([(-.09,.814),(-.066,.816),(-.055,.80)],'Bronze')
plinth.lathe([(.20,.69),(.235,.69),(.25,.62)],'Basalt')
for i in range(8):
    angle=i*math.tau/8
    plinth.box((.65*math.cos(angle),.65*math.sin(angle),.065),(.20,.115,.28),.018,'Bronze',angle)
plinth_obj=plinth.finish('SM_PG_CrystalPlinth',(1.8,1.8,.5))

crystal=MeshBuilder()
crystal.crystal((-.06,0,-1.27),.285,2.56,.19,.12)
crystal.crystal((.20,.09,-1.28),.135,1.47,.75,.06)
crystal.crystal((-.22,-.15,-1.26),.145,1.17,.10,-.025)
crystal.crystal((.09,-.20,-1.26),.12,.81,.28,.055)
crystal.lathe([(-1.30,.34),(-1.22,.37),(-1.14,.34)],'Bronze',sides=8)
crystal_obj=crystal.finish('SM_PG_CrystalCluster',(.85,.85,2.6))

seal=MeshBuilder()
seal.ring((0,0),.68,.025,.009,'Bronze',segments=64)
seal.ring((0,0),.62,.016,.009,'MintLight',segments=64)
for i in range(8):
    a=i*math.tau/8
    seal.box((.66*math.cos(a),.66*math.sin(a),.012),(.068,.068,.012),.002,'MintLight',a+math.pi/4)
seal_obj=seal.finish('SM_PG_RuneSeal',(1,1,1))

objects=[floor_obj,wall_obj,plinth_obj,crystal_obj,seal_obj]
manifest={'schema':1,'materials':PALETTE,'meshes':{}}
for obj in objects:
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active=obj
    collision=None
    if obj in (floor_obj,wall_obj):
        mesh=bpy.data.meshes.new('UBX_'+obj.name+'_00')
        mesh.from_pydata([(x,y,z) for z in (0,1) for y in (0,1) for x in (0,1)],[],[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)])
        collision=bpy.data.objects.new(mesh.name,mesh); sources.objects.link(collision)
        collision.select_set(True)
    export=OUT/(obj.name+'.fbx')
    # FBX -> UE changes handedness by negating Y. Mirror only during export so
    # existing positive-quadrant UE pivots and actor transforms remain exact.
    export_objects=[obj]+([collision] if collision else [])
    for item in export_objects:
        item.data.transform(Matrix.Diagonal((1,-1,1,1))); item.data.flip_normals()
    try:
        assert 'FINISHED' in bpy.ops.export_scene.fbx(filepath=str(export),use_selection=True,object_types={'MESH'},bake_anim=False,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',mesh_smooth_type='FACE',use_triangles=True)
    finally:
        for item in export_objects:
            item.data.transform(Matrix.Diagonal((1,-1,1,1))); item.data.flip_normals()
    obj.data.calc_loop_triangles()
    bounds=[list(map(float,v)) for v in obj.bound_box]
    manifest['meshes'][obj.name]={'file':export.name,'slots':[slot.name for slot in obj.data.materials], 'triangles':len(obj.data.loop_triangles), 'vertices':len(obj.data.vertices), 'bounds_m':[list(map(min,zip(*bounds))),list(map(max,zip(*bounds)))]}
    obj.hide_render=True
    obj.hide_set(True)
    if collision:
        collision.hide_render=True; collision.hide_set(True)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')

def instance(source,name,location,scale,yaw=0,purple=False):
    obj=bpy.data.objects.new(name,source.data); preview.objects.link(obj)
    obj.location=location; obj.scale=scale; obj.rotation_euler.z=yaw
    if purple:
        for slot in obj.material_slots:
            original=slot.material
            slot.link='OBJECT'
            slot.material=materials['Violet'] if original==materials['Mint'] else materials['VioletLight'] if original==materials['MintLight'] else original
    return obj

instance(floor_obj,'PG_Preview_Floor',(0,0,-.4328433),(30,30,.4))
for name,loc,yaw in [('North',(0,29,0),0),('South',(0,0,0),0),('West',(.48,0,0),math.pi/2),('East',(30,0,0),math.pi/2)]:
    instance(wall_obj,'PG_Preview_'+name,loc,(30,1,4),yaw)
for i in range(8):
    a=i*math.pi/4; x,y=15+math.cos(a)*12.5,15+math.sin(a)*12.5
    instance(plinth_obj,'PG_Preview_Plinth_'+str(i),(x,y,.25),(1.8,1.8,.5))
    instance(crystal_obj,'PG_Preview_Crystal_'+str(i),(x,y,1.6),(.85,.85,2.6),purple=i%2==1)
    instance(seal_obj,'PG_Preview_Seal_'+str(i),(x,y,.5),(1,1,1),purple=i%2==1)
camera_data=bpy.data.cameras.new('PG_ArtCamera')
camera=bpy.data.objects.new('PG_ArtCamera',camera_data); preview.objects.link(camera)
camera.location=(38,-27,40)
camera.rotation_euler=(Vector((15,15,0))-camera.location).to_track_quat('-Z','Y').to_euler()
camera_data.type='ORTHO'; camera_data.ortho_scale=46; scene.camera=camera
sun_data=bpy.data.lights.new('PG_ArtSun','SUN'); sun_data.energy=3.0; sun_data.angle=.3
sun=bpy.data.objects.new('PG_ArtSun',sun_data); preview.objects.link(sun); sun.rotation_euler=(.55,-.4,-.4)
world=bpy.data.worlds.new('PG_ArtWorld'); world.use_nodes=True
world.node_tree.nodes['Background'].inputs[0].default_value=(.16,.20,.28,1)
world.node_tree.nodes['Background'].inputs[1].default_value=.45
scene.world=world
scene.render.engine='CYCLES'; scene.cycles.samples=32; scene.cycles.use_denoising=True
scene.render.resolution_x=1400; scene.render.resolution_y=1100; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(OUT/'Preview_Blender.png')
scene.view_settings.view_transform='AgX'
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.region_3d.view_perspective='CAMERA'
bpy.context.view_layer.update()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'PG_RogueEnvironment.blend'),copy=True)
result={'exported':manifest['meshes'],'blend':str(OUT/'PG_RogueEnvironment.blend')}
