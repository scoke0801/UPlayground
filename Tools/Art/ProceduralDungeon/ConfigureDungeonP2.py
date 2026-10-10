"""Author the P2 room kit, real PCG graph and isolated material copies; preserve source art."""
import hashlib
import json
import math
import shutil
import struct
import time
import wave
from datetime import datetime
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/QA/ProceduralDungeon/P2'
DEST='/Game/Dungeons/ForestSanctuary'
E=u.EditorAssetLibrary
LIB=u.MaterialEditingLibrary
OUT.mkdir(parents=True,exist_ok=True)
backup=OUT/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
for relative in ['Content/Dungeons/ForestSanctuary','Content/DataCenter/Progression/DA_PGProgression.uasset','Content/Maps/L_PG_ProceduralDungeon.umap']:
    source=ROOT/relative
    target=backup/relative
    target.parent.mkdir(parents=True,exist_ok=True)
    if source.is_dir(): shutil.copytree(source,target)
    else: shutil.copy2(source,target)

def save(obj):
    assert E.save_loaded_asset(obj,only_if_is_dirty=False),obj.get_path_name()

def asset(name,cls,factory,folder=DEST):
    found=u.load_asset(folder+'/'+name)
    if not found: found=u.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,cls,factory)
    assert found,name
    return found

def data_asset(name,cls,folder=DEST):
    factory=u.DataAssetFactory(); factory.set_editor_property('data_asset_class',cls)
    return asset(name,cls,factory,folder)

def expr(material,cls): return LIB.create_material_expression(material,cls)
def wire(source,target,pin='',output=''):
    assert LIB.connect_material_expressions(source,output,target,pin)
def custom(material,names,code,kind=u.CustomMaterialOutputType.CMOT_FLOAT3):
    node=expr(material,u.MaterialExpressionCustom)
    pins=[]
    for name in names:
        pin=u.CustomInput(); pin.set_editor_property('input_name',name); pins.append(pin)
    node.set_editor_property('inputs',pins); node.set_editor_property('output_type',kind); node.set_editor_property('code',code)
    return node

def stone(name,color):
    mat=asset(name,u.Material,u.MaterialFactoryNew(),DEST+'/Materials')
    LIB.delete_all_material_expressions(mat)
    pos=expr(mat,u.MaterialExpressionWorldPosition)
    code='float n=frac(sin(dot(floor(P/19.0),float3(12.9898,78.233,31.21)))*43758.5453); return float3(%f,%f,%f)*(0.82+0.18*n);'%color
    node=custom(mat,['P'],code); wire(pos,node,'P')
    assert LIB.connect_material_property(node,'',u.MaterialProperty.MP_BASE_COLOR)
    rough=expr(mat,u.MaterialExpressionConstant); rough.set_editor_property('r',.87)
    assert LIB.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    LIB.recompile_material(mat); save(mat)
    return mat

stone_mat=stone('M_PGDungeonBoundary',(.115,.14,.135))
inlays=[stone('M_PGInlay_'+str(i),c) for i,c in enumerate([(.19,.22,.20),(.10,.15,.15),(.24,.21,.16),(.14,.17,.21)])]

def copy_material(source):
    if source.get_path_name().startswith(DEST): return source
    suffix=hashlib.sha1(source.get_path_name().encode()).hexdigest()[:8]
    target=DEST+'/Materials/'+source.get_name()+'_'+suffix
    result=u.load_asset(target) or E.duplicate_asset(source.get_path_name(),target)
    if isinstance(result,u.MaterialInstanceConstant):
        LIB.set_material_instance_parent(result,copy_material(source.get_editor_property('parent')))
    save(result)
    return result

def copy_mesh(path):
    source=u.load_asset(path); assert source,path
    target=DEST+'/Meshes/'+source.get_name()
    result=u.load_asset(target) or E.duplicate_asset(path,target)
    for index,entry in enumerate(source.get_editor_property('static_materials')):
        mat=entry.get_editor_property('material_interface')
        if mat: result.set_material(index,copy_material(mat))
    save(result)
    return result

cup=copy_mesh('/Game/ExternalAssets/LevelDesign/RuinedCrypt/Environment/OldCup_01/sm_OldCup_01_01')
definition=u.load_asset(DEST+'/DA_PGForestDungeon'); assert definition
ruins=[p for p in definition.get_editor_property('ruin_meshes')]
# Convert soft references by loading known saved kit paths.
ruins=[u.load_asset(DEST+'/Meshes/'+n) for n in ['sm_ChapelColumn_01_04','sm_Grave_01_01','sm_ChapelWall_03_02']]
cube=u.load_asset('/Engine/BasicShapes/Cube'); cylinder=u.load_asset('/Engine/BasicShapes/Cylinder')

def piece(mesh,location,scale,yaw=0,material=None,occluder=False):
    p=u.PGDungeonModulePiece()
    p.set_editor_property('mesh',mesh)
    if material: p.set_editor_property('material',material)
    p.set_editor_property('transform',u.Transform(location=u.Vector(*location),rotation=u.Rotator(pitch=0,yaw=yaw,roll=0),scale=u.Vector(*scale)))
    p.set_editor_property('occluder',occluder)
    return p

def prop(mesh,x,y,height,yaw):
    bounds=mesh.get_bounds(); origin=bounds.origin; ext=bounds.box_extent
    scale=min(height/max(1,ext.z*2),230/max(1,ext.x*2,ext.y*2))
    angle=math.radians(yaw)
    ox=origin.x*math.cos(angle)-origin.y*math.sin(angle)
    oy=origin.x*math.sin(angle)+origin.y*math.cos(angle)
    return piece(mesh,(x-ox*scale,y-oy*scale,-(origin.z-ext.z)*scale),(scale,scale,scale),yaw,occluder=True)

roles=[('Entrance',u.PGDungeonRoomRole.ENTRANCE),('Court',u.PGDungeonRoomRole.COMBAT),
       ('Tombs',u.PGDungeonRoomRole.COMBAT),('Cross',u.PGDungeonRoomRole.COMBAT),('Sanctum',u.PGDungeonRoomRole.COMBAT),
       ('Elite',u.PGDungeonRoomRole.ELITE),('Treasure',u.PGDungeonRoomRole.TREASURE),('Boss',u.PGDungeonRoomRole.BOSS)]
modules=[]
for index,(name,role) in enumerate(roles):
    module=data_asset('DA_PGRoom_'+name,u.PGDungeonRoomDefinition,DEST+'/Rooms')
    pieces=[]
    mat=inlays[index%len(inlays)]
    if name in ['Entrance','Sanctum','Boss']:
        # Broad sun-like paving, all flush with the floor and non-colliding.
        for angle in range(0,360,45):
            r=math.radians(angle)
            pieces.append(piece(cube,(math.cos(r)*600,math.sin(r)*600,1.2),(6,1.1,.018),angle,mat))
        pieces.append(piece(cylinder,(0,0,1.1),(6,6,.02),material=mat))
    elif name in ['Cross','Elite']:
        for yaw in [0,90]: pieces.append(piece(cube,(0,0,1.1),(22,2.2,.018),yaw,mat))
    elif name=='Tombs':
        for x in [-550,550]:
            for y in [-600,0,600]: pieces.append(piece(cube,(x,y,1.1),(3,4,.018),material=mat))
    elif name=='Treasure':
        pieces.append(piece(cylinder,(350,350,1.2),(6,6,.02),material=mat))
    else:
        for y in [-750,750]: pieces.append(piece(cube,(0,y,1.1),(20,1.2,.018),material=mat))
    for corner,(x,y) in enumerate([(-1190,-1190),(1190,-1190),(-1190,1190),(1190,1190)]):
        if name=='Court' and corner%2: continue
        mesh=ruins[(index+corner)%3]
        pieces.append(prop(mesh,x,y,200 if name!='Boss' else 300,corner*90))
    for key,value in dict(module_id=name,role=role,room_size=2800.,allowed_quarter_turns=[0,1,2,3],pieces=pieces).items():
        module.set_editor_property(key,value)
    save(module); modules.append(module)

graph=asset('PCG_PGDungeonEnvironment',u.PCGGraph,u.PCGGraphFactory())
for node in list(graph.get_editor_property('nodes')): graph.remove_node(node)
node,settings=graph.add_node_of_type(u.PGDungeonScatterSettings)
graph.add_edge(node,'Out',graph.get_editor_property('output_node'),'Out')
save(graph)

# Per-instance custom data slot 2 drives only dungeon copies; retain authored masked coverage.
faded=[]
for path in E.list_assets(DEST+'/Materials',recursive=True,include_folder=False):
    mat=u.load_asset(path)
    if not isinstance(mat,u.Material): continue
    if any(n.get_editor_property('desc')=='PG dungeon occlusion' for n in LIB.get_material_expressions(mat)): continue
    opacity=LIB.get_material_property_input_node(mat,u.MaterialProperty.MP_OPACITY_MASK)
    output=LIB.get_material_property_input_node_output_name(mat,u.MaterialProperty.MP_OPACITY_MASK)
    if mat.get_editor_property('blend_mode')==u.BlendMode.BLEND_OPAQUE or not opacity:
        opacity=expr(mat,u.MaterialExpressionConstant); opacity.set_editor_property('r',1); output=''
    value=expr(mat,u.MaterialExpressionPerInstanceCustomData); value.set_editor_property('data_index',2)
    interp=expr(mat,u.MaterialExpressionVertexInterpolator); wire(value,interp)
    visible=expr(mat,u.MaterialExpressionOneMinus); wire(interp,visible)
    dither=expr(mat,u.MaterialExpressionMaterialFunctionCall)
    dither.set_material_function(u.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA'))
    pin=next(str(n) for n in LIB.get_material_expression_input_names(dither) if 'Alpha' in str(n))
    wire(visible,dither,pin)
    mask=custom(mat,['Coverage','Dither','Fade'],'return Coverage * lerp(Fade <= 0 ? 1 : Dither, 1, IsShadowDepthShader());',u.CustomMaterialOutputType.CMOT_FLOAT1)
    mask.set_editor_property('desc','PG dungeon occlusion')
    wire(opacity,mask,'Coverage',output); wire(dither,mask,'Dither'); wire(interp,mask,'Fade')
    mat.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    assert LIB.connect_material_property(mask,'',u.MaterialProperty.MP_OPACITY_MASK)
    LIB.recompile_material(mat); save(mat); faded.append(path)

gate=asset('M_PGDungeonSeal',u.Material,u.MaterialFactoryNew(),DEST+'/Materials')
LIB.delete_all_material_expressions(gate)
uv=expr(gate,u.MaterialExpressionTextureCoordinate)
t=expr(gate,u.MaterialExpressionTime)
color=custom(gate,['UV','T'],'float bars=step(frac(UV.x*9),0.16); float edge=step(UV.y,0.05)+step(0.95,UV.y); return float3(1.0,0.32,0.055)*(bars*2+edge)*(0.85+0.15*sin(T*2));')
wire(uv,color,'UV'); wire(t,color,'T')
mask=custom(gate,['UV'],'return max(step(frac(UV.x*9),0.16),max(step(UV.y,0.05),step(0.95,UV.y)));',u.CustomMaterialOutputType.CMOT_FLOAT1)
wire(uv,mask,'UV')
gate.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED)
gate.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
gate.set_editor_property('two_sided',True)
assert LIB.connect_material_property(color,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
assert LIB.connect_material_property(mask,'',u.MaterialProperty.MP_OPACITY_MASK)
LIB.recompile_material(gate); save(gate)

sound_path=OUT/'PG_DungeonSeal.wav'
with wave.open(str(sound_path),'wb') as stream:
    stream.setparams((1,2,24000,0,'NONE','not compressed'))
    values=[]
    for i in range(16800):
        t=i/24000; attack=min(1,t/.025); envelope=attack*math.exp(-t*6)
        v=sum(math.sin(2*math.pi*f*t)*a for f,a in [(110,.45),(221,.22),(331,.12),(557,.06)])*envelope
        values.append(struct.pack('<h',int(max(-1,min(1,v))*18000)))
    stream.writeframes(b''.join(values))
task=u.AssetImportTask(); task.set_editor_property('filename',str(sound_path)); task.set_editor_property('destination_path',DEST+'/Audio')
task.set_editor_property('automated',True); task.set_editor_property('replace_existing',True); task.set_editor_property('save',True)
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound=u.load_asset(DEST+'/Audio/PG_DungeonSeal'); assert sound

catalog=u.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
pools=list(catalog.get_editor_property('drop_pools'))
base=next((p for p in pools if str(p.get_editor_property('id'))=='Rogue.Warden'),None)
assert base,'Expected existing elite loot budget'
pool=u.PGDropPool(); pool.set_editor_property('id','DungeonTreasure'); pool.set_editor_property('guaranteed',True)
pool.set_editor_property('drop_chance',1.); pool.set_editor_property('entries',base.get_editor_property('entries'))
catalog.set_editor_property('drop_pools',[p for p in pools if str(p.get_editor_property('id'))!='DungeonTreasure']+[pool]); save(catalog)

for key,value in dict(content_version=2,room_modules=modules,decoration_graph=graph,wall_material=stone_mat,
    treasure_mesh=cup,treasure_material=None,branch_loot_pool='DungeonTreasure',gate_mesh=cube,gate_material=gate,gate_sound=sound).items():
    definition.set_editor_property(key,value)
save(definition)
report=dict(status='SAVED',backup=str(backup),modules=[m.get_path_name() for m in modules],graph=graph.get_path_name(),fade_materials=faded)
level=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/L_PG_ProceduralDungeon')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
generator=u.GameplayStatics.get_actor_of_class(world,u.PGDungeonGenerator)
assert generator and generator.generate_preview(),generator.get_editor_property('last_error') if generator else 'No generator'
u.EditorPythonScripting.set_keep_python_script_alive(True)
started=time.monotonic()
finished=False
def wait_for_preview(dt):
    global finished
    if finished: return
    state=generator.get_editor_property('state')
    if state==u.PGDungeonState.ENVIRONMENT and time.monotonic()-started<60: return
    finished=True
    try:
        assert state==u.PGDungeonState.IDLE,generator.get_editor_property('last_error')
        assert generator.get_editor_property('decoration_point_count')>0
        assert u.EditorLoadingAndSavingUtils.save_map(world,'/Game/Maps/L_PG_ProceduralDungeon')
        report['preview_points']=generator.get_editor_property('decoration_point_count')
        (OUT/'assets.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        u.log('PGDungeonP2 ASSETS PASS')
    except BaseException as error:
        u.log_error(str(error))
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()
handle=u.register_slate_post_tick_callback(wait_for_preview)
