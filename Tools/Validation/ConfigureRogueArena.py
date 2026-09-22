"""Create a stylized arena from the existing tested collision/nav layout.
New materials use procedural geometry/color; no external art dependencies.
"""
import unreal, os, json, math
OUT='/Game/DataCenter/RoguelikeMVP'
MAP='/Game/Maps/RogueArena'
tools=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
def material(name,color,pattern=False,glow=False):
    path=OUT+'/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path): return unreal.load_asset(path)
    mat=tools.create_asset(name,OUT,unreal.Material,unreal.MaterialFactoryNew())
    base=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector)
    base.set_editor_property('constant',unreal.LinearColor(*color,1))
    output=base
    if pattern:
        pos=lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition,-600,100)
        code=lib.create_material_expression(mat,unreal.MaterialExpressionCustom,-250,100)
        inp=unreal.CustomInput(); inp.set_editor_property('input_name','P')
        code.set_editor_property('inputs',[inp]); code.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        code.set_editor_property('code','float2 uv=P.xy/320.; float2 f=frac(uv); float edge=step(.018,min(min(f.x,1-f.x),min(f.y,1-f.y))); float v=frac(sin(dot(floor(uv),float2(12.9898,78.233)))*43758.5453); float3 tile=lerp(float3(.12,.16,.23),float3(.22,.27,.36),step(.5,v)); return lerp(float3(.055,.09,.14),tile,edge);')
        lib.connect_material_expressions(pos,'',code,'P'); output=code
    lib.connect_material_property(output,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=lib.create_material_expression(mat,unreal.MaterialExpressionConstant)
    rough.set_editor_property('r',.88); lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    if glow: lib.connect_material_property(base,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(mat); assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat
floor=material('M_ArenaSlate',(.18,.22,.3),True)
wall=material('M_ArenaWall',(.10,.13,.22))
mint=material('M_ArenaMint',(.18,.85,.65),False,True)
lavender=material('M_ArenaLavender',(.45,.28,.8),False,True)
stone=material('M_ArenaPillar',(.22,.24,.34))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not unreal.EditorAssetLibrary.does_asset_exist(MAP):
    assert level.new_level_from_template(MAP,'/Game/Maps/StageDevMap')
else: assert level.load_level(MAP)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert any(a.get_components_by_class(unreal.StaticMeshComponent) for a in actors.get_all_level_actors()), 'Template geometry was not loaded'
inventory=[]
for actor in actors.get_all_level_actors():
    if actor.get_actor_label().startswith('RogueDecor_'): actors.destroy_actor(actor); continue
    comps=actor.get_components_by_class(unreal.StaticMeshComponent)
    for comp in comps:
        if 'sky' in actor.get_actor_label().lower():
            mesh=comp.get_editor_property('static_mesh')
            if mesh: comp.set_material(0,mesh.get_material(0))
            continue
        bounds=actor.get_actor_bounds(False)
        is_floor=bounds[1].z < max(bounds[1].x,bounds[1].y)*.15
        for index in range(max(1,comp.get_num_materials())): comp.set_material(index,floor if is_floor else wall)
    inventory.append(dict(name=actor.get_actor_label(),location=str(actor.get_actor_location()),bounds=str(actor.get_actor_bounds(False))))

# Decoration remains outside the central fighting area and never changes collision/navigation.
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
starts=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PlayerStart)
center=starts[0].get_actor_location() if starts else unreal.Vector(0,0,100)
for actor in actors.get_all_level_actors():
    origin,extent=actor.get_actor_bounds(False)
    if extent.x>1000 and extent.y>1000 and extent.z<100:
        center=unreal.Vector(origin.x,origin.y,origin.z+extent.z); break
def prop(name,mesh,location,scale,mat,rotation=unreal.Rotator()):
    actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,location,rotation)
    actor.set_actor_label('RogueDecor_'+name)
    comp=actor.static_mesh_component
    comp.set_static_mesh(unreal.load_asset(mesh)); comp.set_material(0,mat)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property('cast_shadow',False)
    actor.set_actor_scale3d(scale)
for i in range(8):
    angle=i*math.pi/4
    x,y=center.x+math.cos(angle)*1250,center.y+math.sin(angle)*1250
    prop('Plinth'+str(i),'/Engine/BasicShapes/Cylinder',unreal.Vector(x,y,25),unreal.Vector(1.8,1.8,.5),stone)
    prop('Crystal'+str(i),'/Engine/BasicShapes/Cone',unreal.Vector(x,y,160),unreal.Vector(.85,.85,2.6),mint if i%2==0 else lavender)
    prop('Band'+str(i),'/Engine/BasicShapes/Cube',unreal.Vector(x,y,50),unreal.Vector(1.8,.16,.1),mint,unreal.Rotator(0,i*45,0))
assert level.save_current_level()
with open(os.path.join(unreal.Paths.project_saved_dir(),'RoguelikeMVP/arena.json'),'w',encoding='utf-8') as f: json.dump(inventory,f,indent=2)
catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
items=list(catalog.get_editor_property('items'))
for item in items:
    item.set_editor_property('icon_panel',{15004:0,15005:1,15006:2}.get(item.get_editor_property('id'),1))
catalog.set_editor_property('items',items)
if '-PGSkipCatalogSave' not in unreal.SystemLibrary.get_command_line(): assert unreal.EditorAssetLibrary.save_loaded_asset(catalog)
unreal.log('PGRogue ARENA COMPLETE '+MAP)
