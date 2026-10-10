"""Build the isolated P0 map; preserve source sanctuary art and gameplay assets."""
import hashlib
import json
from datetime import datetime
from pathlib import Path
import shutil
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT/'Saved/QA/ProceduralDungeon'
OUT.mkdir(parents=True, exist_ok=True)
DEST = '/Game/Dungeons/ForestSanctuary'
MAP = '/Game/Maps/L_PG_ProceduralDungeon'
SOURCE = '/Game/Maps/L_PG_ForestRuins'
E = u.EditorAssetLibrary
A = u.get_editor_subsystem(u.EditorActorSubsystem)
L = u.get_editor_subsystem(u.LevelEditorSubsystem)
source_file = ROOT/'Content/Maps/L_PG_ForestRuins.umap'
source_hash = hashlib.sha256(source_file.read_bytes()).hexdigest()
backup = OUT/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
for relative in ['Content/Maps/L_PG_ProceduralDungeon.umap', 'Content/Dungeons/ForestSanctuary']:
    path = ROOT/relative
    if path.exists():
        backup.mkdir(parents=True, exist_ok=True)
        if path.is_dir(): shutil.copytree(path, backup/path.name)
        else: shutil.copy2(path, backup/path.name)

def asset(name, cls, factory):
    result = u.load_asset(DEST+'/'+name)
    if not result: result = u.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, cls, factory)
    assert result, name
    return result

def save(obj):
    assert E.save_loaded_asset(obj, only_if_is_dirty=False), obj.get_path_name()

assert L.load_level(SOURCE)
source_world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
source_mode = u.get_default_object(source_world.get_world_settings().get_editor_property('default_game_mode'))
mode_factory = u.BlueprintFactory()
mode_factory.set_editor_property('parent_class', u.PGGameModeBase)
mode = asset('BP_PGDungeonPreviewMode', u.Blueprint, mode_factory)
mode_cdo = u.get_default_object(mode.generated_class())
for key in ['default_pawn_class', 'player_controller_class', 'hud_class', 'player_state_class']:
    mode_cdo.set_editor_property(key, source_mode.get_editor_property(key))
save(mode)

# Copy the existing tonal palette, textures and world-space paving pattern.
ground_path = DEST+'/M_PGDungeonGround'
if not E.does_asset_exist(ground_path):
    ground = E.duplicate_asset('/Game/Environment/ForestRuins/Sanctuary/M_PG_SanctuaryGround', ground_path)
    assert ground
    lib = u.MaterialEditingLibrary
    # The source material is kept intact; same stone/moss shading is the P0 baseline.
else: ground = u.load_asset(ground_path)
# Recenter the existing court mask per floor instance; texture UVs stay world-scaled.
lib=u.MaterialEditingLibrary
expressions=lib.get_material_expressions(ground)
locals_=[n for n in expressions if isinstance(n,u.MaterialExpressionCustom) and n.get_editor_property('description')=='PG room coordinates']
if locals_:
    local=locals_[0]
else:
    local=lib.create_material_expression(ground,u.MaterialExpressionCustom)
    local.set_editor_property('description','PG room coordinates')
    pins=[]
    for name in ['P','O']:
        pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    local.set_editor_property('inputs',pins)
    local.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3)
    local.set_editor_property('code','return float3(P.xy-O.xy,P.z);')
pos=next(n for n in expressions if isinstance(n,u.MaterialExpressionWorldPosition))
assert lib.connect_material_expressions(pos,'',local,'P')
# ObjectPositionWS is the whole ISM bounds in this material path, not the room.
# The generator writes each floor's world center to per-instance custom data 0/1.
xy=lib.create_material_expression(ground,u.MaterialExpressionAppendVector)
for index,pin in enumerate(['A','B']):
    value=lib.create_material_expression(ground,u.MaterialExpressionPerInstanceCustomData)
    value.set_editor_property('data_index',index)
    assert lib.connect_material_expressions(value,'',xy,pin)
xyz=lib.create_material_expression(ground,u.MaterialExpressionAppendVector)
zero=lib.create_material_expression(ground,u.MaterialExpressionConstant)
assert lib.connect_material_expressions(xy,'',xyz,'A')
assert lib.connect_material_expressions(zero,'',xyz,'B')
interp=lib.create_material_expression(ground,u.MaterialExpressionVertexInterpolator)
assert lib.connect_material_expressions(xyz,'',interp,'')
assert lib.connect_material_expressions(interp,'',local,'O')
for node in expressions:
    if node!=local and isinstance(node,u.MaterialExpressionCustom) and 'P' in lib.get_material_expression_input_names(node):
        if 'P.xy/420.' not in node.get_editor_property('code'):
            assert lib.connect_material_expressions(local,'',node,'P')
lib.delete_unused_expressions(ground)
ground.set_editor_property('used_with_instanced_static_meshes',True)
u.MaterialEditingLibrary.recompile_material(ground)
save(ground)

# Keep marketplace assets read-only. Instance-compatible copies retain the source
# material graph and parameters, including parents of material instances.
material_copies={}
def instance_material(source):
    key=source.get_path_name()
    if key in material_copies:return material_copies[key]
    path=DEST+'/Materials/'+source.get_name()
    copy=u.load_asset(path) if E.does_asset_exist(path) else E.duplicate_asset(key.split('.')[0],path)
    assert copy,key
    material_copies[key]=copy
    if isinstance(copy,u.MaterialInstanceConstant):
        copy.set_editor_property('parent',instance_material(source.get_editor_property('parent')))
    elif isinstance(copy,u.Material):
        copy.set_editor_property('used_with_instanced_static_meshes',True)
        u.MaterialEditingLibrary.recompile_material(copy)
    save(copy)
    return copy

def instance_mesh(path):
    source=u.load_asset(path);assert source,path
    dest=DEST+'/Meshes/'+source.get_name()
    copy=u.load_asset(dest) if E.does_asset_exist(dest) else E.duplicate_asset(path,dest)
    for index,slot in enumerate(source.static_materials):
        if slot.material_interface:copy.set_material(index,instance_material(slot.material_interface))
    save(copy)
    return copy

definition = asset('DA_PGForestDungeon', u.PGDungeonDefinition, u.DataAssetFactory())
for key,value in dict(min_rooms=8, max_rooms=12, room_size=2800., corridor_length=1000.,
        corridor_width=800., wall_height=180., props_per_room=4, max_attempts=3, navigation_timeout=30.,
        block_mesh=u.load_asset('/Engine/BasicShapes/Cube'), ground_material=ground,
        wall_material=instance_material(u.load_asset('/Game/ExternalAssets/LevelDesign/RuinedCrypt/Environment/ChapelWall_03/mi_ChapelWall_03_01')),
        exterior_meshes=[instance_mesh('/Game/Environment/ForestRuins/Meshes/'+p) for p in [
            'SM_PGFR_HD_Osmanthus_01','SM_PGFR_HD_Conifer_Twisted_01']],
        ruin_meshes=[instance_mesh('/Game/ExternalAssets/LevelDesign/RuinedCrypt/Environment/'+p) for p in [
            'ChapelColumn_01/sm_ChapelColumn_01_04','Grave_01/sm_Grave_01_01',
            'ChapelWall_03/sm_ChapelWall_03_02']]).items():
    definition.set_editor_property(key,value)
save(definition)

assert (L.load_level(MAP) if E.does_asset_exist(MAP) else L.new_level_from_template(MAP,SOURCE))
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
# Preserve the exact lights, postprocess and atmospheric settings of the test map.
keep = (u.Light,u.SkyLight,u.PostProcessVolume,u.ExponentialHeightFog,u.SkyAtmosphere,u.VolumetricCloud,u.WorldSettings)
for actor in list(A.get_all_level_actors()):
    if isinstance(actor, keep) or actor.get_class().get_name() in ['Brush','DefaultPhysicsVolume','LevelScriptActor']:
        continue
    assert A.destroy_actor(actor), actor.get_name()
world.get_world_settings().set_editor_property('default_game_mode',mode.generated_class())
for light in A.get_all_level_actors():
    if isinstance(light,u.DirectionalLight):
        light.light_component.set_editor_property('forward_shading_priority',1 if light.get_actor_label()=='숲의 주광원' else 0)
generator = A.spawn_actor_from_class(u.PGDungeonGenerator,u.Vector())
generator.set_actor_label('폐성소 절차 생성기')
generator.set_editor_property('definition',definition)
generator.set_editor_property('seed',101026)
assert generator.generate_preview(), generator.get_editor_property('last_error')
start = A.spawn_actor_from_class(u.PlayerStart,u.Vector(0,0,110))
start.set_actor_label('던전 입구')
nav = A.spawn_actor_from_class(u.NavMeshBoundsVolume,u.Vector(0,0,200))
origin,extent = nav.get_actor_bounds(False)
assert min(extent.x,extent.y,extent.z)>0
nav.set_actor_scale3d(u.Vector(32000/extent.x,32000/extent.y,700/extent.z))
recast = u.PGEditorProbeTools.ensure_editor_navigation(world,nav)
assert recast
recast.set_editor_property('runtime_generation',u.RuntimeGenerationType.DYNAMIC)
recast.set_editor_property('force_rebuild_on_load',True)
assert u.EditorLoadingAndSavingUtils.save_map(world,MAP)
assert hashlib.sha256(source_file.read_bytes()).hexdigest()==source_hash
report = dict(status='SAVED',map=MAP,definition=definition.get_path_name(),source_map_sha256=source_hash,
    scope='P0 geometry and navigation; no StageManager or rewards',backup=str(backup),
    source_art='/Game/ExternalAssets/LevelDesign/RuinedCrypt',seed=101026)
(OUT/'map.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('PGDungeon MAP SAVED')
