"""Original low-poly guardian armor. Blender CLI; metres, shield faces +X."""
import json
import math
import random
import struct
import wave
from pathlib import Path
import bpy
import bmesh
from mathutils import Vector

OUT = Path(__file__).resolve().parent
scene = bpy.data.scenes.new('PG_GuardianArt')
bpy.context.window.scene = scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1
palette = {
    'iron': (.055, .085, .125, 0), 'edge': (.48, .32, .12, 0),
    'steel': (.25, .34, .42, 0), 'ivory': (.65, .70, .68, 0),
    'rune': (.10, .60, .9, 1), 'dark': (.018, .03, .05, 0),
}
mat = bpy.data.materials.new('M_PG_GuardianArmor')
mat.use_nodes = True
shader = mat.node_tree.nodes.get('Principled BSDF')
color = mat.node_tree.nodes.new('ShaderNodeVertexColor'); color.layer_name = 'Col'
mat.node_tree.links.new(color.outputs['Color'], shader.inputs['Base Color'])
mat.node_tree.links.new(color.outputs['Color'], shader.inputs['Emission Color'])
mat.node_tree.links.new(color.outputs['Alpha'], shader.inputs['Emission Strength'])
shader.inputs['Metallic'].default_value = .65
shader.inputs['Roughness'].default_value = .38
objects = []
manifest = {'schema':1, 'material':'M_PG_GuardianArmor', 'meshes':[]}

def model(name, build):
    vertices, faces, colors = [], [], []
    def add(verts, polys, color):
        n = len(vertices); vertices.extend(verts)
        faces.extend(tuple(n+i for i in face) for face in polys)
        colors.extend([palette[color]] * len(polys))
    def plate(outline, back, front, color):
        n = len(outline)
        verts = [(x,y,z) for x in (back,front) for y,z in outline]
        polys = [tuple(reversed(range(n))),tuple(range(n,n*2))]
        polys += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        add(verts,polys,color)
    def gem(center, radii, color):
        x,y,z = center; a,b,c = radii
        add([(x+a,y,z),(x-a,y,z),(x,y+b,z),(x,y-b,z),(x,y,z+c),(x,y,z-c)],
            [(0,2,4),(0,4,3),(0,3,5),(0,5,2),(1,4,2),(1,3,4),(1,5,3),(1,2,5)],color)
    build(plate,gem,add)
    mesh = bpy.data.meshes.new(name); mesh.from_pydata(vertices,[],faces); mesh.materials.append(mat)
    attribute = mesh.color_attributes.new(name='Col',type='FLOAT_COLOR',domain='CORNER')
    for poly,rgba in zip(mesh.polygons,colors):
        for i in poly.loop_indices: attribute.data[i].color = rgba
    bm = bmesh.new(); bm.from_mesh(mesh); bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces)); bm.to_mesh(mesh); bm.free()
    uv = mesh.uv_layers.new(name='UVMap')
    for poly in mesh.polygons:
        for i in poly.loop_indices:
            co = mesh.vertices[mesh.loops[i].vertex_index].co
            uv.data[i].uv = (co.y+.5,co.z+.5)
    obj = bpy.data.objects.new(name,mesh); scene.collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH'},
        bake_anim=False,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_NONE',mesh_smooth_type='FACE',use_triangles=True)
    mesh.calc_loop_triangles()
    points = [list(p) for p in obj.bound_box]
    manifest['meshes'].append(dict(name=name,fbx=name+'.fbx',triangles=len(mesh.loop_triangles),
        bounds_m=[list(map(min,zip(*points))),list(map(max,zip(*points)))]))
    objects.append(obj)
    return obj

outline = [(-.27,.44),(.27,.44),(.33,.27),(.28,-.28),(0,-.55),(-.28,-.28),(-.33,.27)]
def shield(p,g,a):
    p(outline,-.025,.018,'dark')
    p(outline,.018,.045,'edge')
    p([(y*.88,z*.91) for y,z in outline],.045,.066,'steel')
    p([(y*.78,z*.84) for y,z in outline],.066,.073,'iron')
    # Segmented ivory crest, a central ridge and a broken diamond rune.
    for sign in (-1,1):
        p([(sign*.04,.31),(sign*.22,.34),(sign*.19,.19),(sign*.08,.12)],.074,.084,'ivory')
        p([(sign*.055,.12),(sign*.15,.02),(sign*.055,-.17),(sign*.08,-.035)],.085,.090,'rune')
        for z in (-.20,.23): g((.066,sign*.255,z),(.020,.018,.020),'edge')
    p([(-.023,.36),(.023,.36),(.025,-.27),(0,-.40),(-.025,-.27)],.074,.098,'edge')
    g((.102,0,.045),(.052,.07,.12),'rune')
    # Back grip stays behind the face and gives the hand a physical attachment.
    p([(-.05,.07),(.05,.07),(.05,-.07),(-.05,-.07)],-.075,-.025,'dark')

def pauldron(p,g,a):
    # Three overlapping plates. Local +Z points up after socket calibration.
    for index in range(3):
        y = index*.07
        points = [(-.17-y,.08-index*.08),(.14+y,.10-index*.08),(.20+y,-.015-index*.08),
                  (.12+y,-.075-index*.08),(-.18-y,-.06-index*.08)]
        p(points,-.095,.095-index*.015,'edge')
        p([(v*.92,z+.018) for v,z in points],.096-index*.015,.106-index*.015,'iron' if index else 'steel')
    g((.13,-.035,.055),(.09,.055,.115),'ivory')

shield_obj = model('SM_PG_GuardianShield',shield)
shoulder = model('SM_PG_GuardianPauldron',pauldron)
shoulder.location = (.03,.72,.24)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')

# Short original metal/charge cues; deterministic, unclipped PCM sources.
for name,duration,frequencies in [
    ('S_PG_GuardClang',.22,[470,923,1471,2243]), ('S_PG_GuardWindup',.40,[190,285,570]),
    ('S_PG_GuardLock',.10,[850,1700]), ('S_PG_GuardRecover',.25,[120,233,402]),
    ('S_PG_GuardSlam',.22,[85,177,355,710])]:
    rng = random.Random(15103); rate = 44100; samples = []
    for i in range(int(rate*duration)):
        t=i/rate; envelope=min(1,t/.003)*math.exp(-t/(duration*.24))
        tone=sum(math.sin(math.tau*f*t)/(1+j*.5) for j,f in enumerate(frequencies))/len(frequencies)
        value=(tone*.65+rng.uniform(-1,1)*.16*math.exp(-t/.025))*envelope
        samples.append(struct.pack('<h',int(max(-.95,min(.95,value))*32767)))
    with wave.open(str(OUT/(name+'.wav')),'wb') as stream:
        stream.setnchannels(1); stream.setsampwidth(2); stream.setframerate(rate); stream.writeframes(b''.join(samples))

camera_data=bpy.data.cameras.new('Camera'); camera=bpy.data.objects.new('Camera',camera_data); scene.collection.objects.link(camera)
camera.location=(2.4,-2.1,1.7); target=Vector((0,.20,0))
camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler(); camera_data.type='ORTHO'; camera_data.ortho_scale=1.9; scene.camera=camera
for pos,energy,size in [((2,-2,3),250,3),((-1,2,2),180,2)]:
    light_data=bpy.data.lights.new('Studio','AREA'); light_data.energy=energy; light_data.size=size
    light=bpy.data.objects.new('Studio',light_data); scene.collection.objects.link(light); light.location=pos
    light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
world=bpy.data.worlds.new('GuardianWorld'); world.use_nodes=True; world.node_tree.nodes['Background'].inputs[0].default_value=(.03,.045,.07,1); world.node_tree.nodes['Background'].inputs[1].default_value=.4; scene.world=world
scene.render.engine='CYCLES'; scene.cycles.samples=32; scene.cycles.use_denoising=True
scene.render.resolution_x=1000; scene.render.resolution_y=800; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(OUT/'Preview_Blender.png')
bpy.data.libraries.write(str(OUT/'PG_Guardian.blend'),{scene},fake_user=True)
bpy.ops.render.render(write_still=True)
print(json.dumps(manifest))
