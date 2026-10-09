"""Build an original playable forest courtyard using standalone imported props.

Reads layout.json. Rebuilding only replaces this generated level; earlier versions
are copied to Saved/ForestRuins/Backups. Existing gameplay data is read-only.
"""
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import random
import shutil
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
KIT=ROOT/'Tools/Art/ForestRuins'
OUT=ROOT/'Saved/ForestRuins'
DEST='/Game/Environment/ForestRuins'
CONFIG=json.loads((KIT/'layout.json').read_text(encoding='utf-8'))
IMPORTED=json.loads((OUT/'import.json').read_text(encoding='utf-8'))
MAP=CONFIG['map']
assert IMPORTED['status']=='PASS'
EAL=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
LIB=unreal.MaterialEditingLibrary
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LEVEL=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
RNG=random.Random(CONFIG['seed'])
REPORT=dict(status='RUNNING',map=MAP,resource_only=True,blank_level=True,placements=[],instances={},protected=IMPORTED['protected'])

def save(asset):
    assert asset.get_path_name().startswith(DEST+'/'),asset.get_path_name()
    assert EAL.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()

def vec(value): return [round(getattr(value,axis),4) for axis in ('x','y','z')]

def own(name,cls,factory,folder='Materials'):
    path=DEST+'/'+folder+'/'+name
    obj=unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(name,DEST+'/'+folder,cls,factory)
    assert obj,path
    return obj

def expr(mat,cls): return LIB.create_material_expression(mat,cls)

def constant(mat,value):
    node=expr(mat,unreal.MaterialExpressionConstant)
    node.set_editor_property('r',value)
    return node

def connect(a,pin,b,entry): assert LIB.connect_material_expressions(a,pin,b,entry)

def output(a,pin,entry): assert LIB.connect_material_property(a,pin,entry)

def base_material(name,tint,glow=None):
    mat=own(name,unreal.Material,unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    sample=expr(mat,unreal.MaterialExpressionTextureSample)
    sample.set_editor_property('texture',unreal.load_asset(DEST+'/Textures/T_PGFR_Main'))
    color=expr(mat,unreal.MaterialExpressionVectorParameter)
    color.set_editor_property('parameter_name','BaseTint')
    color.set_editor_property('default_value',unreal.LinearColor(*tint,1))
    multiply=expr(mat,unreal.MaterialExpressionMultiply)
    connect(sample,'RGB',multiply,'A');connect(color,'RGB',multiply,'B')
    output(multiply,'',unreal.MaterialProperty.MP_BASE_COLOR)
    output(constant(mat,.90),'',unreal.MaterialProperty.MP_ROUGHNESS)
    output(constant(mat,0.),'',unreal.MaterialProperty.MP_SPECULAR)
    if glow:
        emission=expr(mat,unreal.MaterialExpressionVectorParameter)
        emission.set_editor_property('parameter_name','GlowColor')
        emission.set_editor_property('default_value',unreal.LinearColor(*glow,1))
        output(emission,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.layout_material_expressions(mat);LIB.recompile_material(mat);save(mat)
    return mat

def ground_material():
    mat=own('M_PGFR_Ground',unreal.Material,unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(mat)
    tex=[]
    for name in ['Mud','Grass']:
        node=expr(mat,unreal.MaterialExpressionTextureSample)
        node.set_editor_property('texture',unreal.load_asset(DEST+'/Textures/T_PGFR_'+name));tex.append(node)
    position=expr(mat,unreal.MaterialExpressionWorldPosition)
    colors=expr(mat,unreal.MaterialExpressionCustom)
    pin=unreal.CustomInput();pin.set_editor_property('input_name','P')
    colors.set_editor_property('inputs',[pin])
    colors.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    hx,hy=[x-50 for x in CONFIG['boundary_half_extent_cm']]
    colors.set_editor_property('code',f'float d=length(P.xy/float2({hx}.,{hy}.)); float g=smoothstep(.76,1.16,d); float trail=min(abs(P.x+250.),abs(P.y-120.)); if(d<1.12)g*=max(.2,smoothstep(130.,330.,trail)); return g;')
    connect(position,'',colors,'P')
    lerp=expr(mat,unreal.MaterialExpressionLinearInterpolate)
    connect(tex[0],'RGB',lerp,'A');connect(tex[1],'RGB',lerp,'B');connect(colors,'',lerp,'Alpha')
    tint=expr(mat,unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property('parameter_name','GroundTint')
    tint.set_editor_property('default_value',unreal.LinearColor(.60,.66,.53,1))
    multiply=expr(mat,unreal.MaterialExpressionMultiply)
    connect(lerp,'',multiply,'A');connect(tint,'RGB',multiply,'B')
    variation=expr(mat,unreal.MaterialExpressionCustom)
    pin=unreal.CustomInput();pin.set_editor_property('input_name','P')
    variation.set_editor_property('inputs',[pin])
    variation.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    variation.set_editor_property('code','return .95+.05*sin(P.x*.004+sin(P.y*.003))*sin(P.y*.0033);')
    connect(position,'',variation,'P')
    final=expr(mat,unreal.MaterialExpressionMultiply)
    connect(multiply,'',final,'A');connect(variation,'',final,'B')
    output(final,'',unreal.MaterialProperty.MP_BASE_COLOR)
    output(constant(mat,1.),'',unreal.MaterialProperty.MP_ROUGHNESS)
    output(constant(mat,0.),'',unreal.MaterialProperty.MP_SPECULAR)
    LIB.layout_material_expressions(mat);LIB.recompile_material(mat);save(mat)
    return mat

def import_terrain():
    for name in ['SM_PGFR_Ground','SM_PGFR_Boundary']:
        task=unreal.AssetImportTask()
        for key,value in dict(filename=str(KIT/(name+'.fbx')),destination_path=DEST+'/Meshes',destination_name=name,
                              automated=True,replace_existing=True,save=True,factory=unreal.FbxFactory()).items():task.set_editor_property(key,value)
        options=unreal.FbxImportUI()
        for key,value in dict(automated_import_should_detect_type=False,mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH,
                              import_mesh=True,import_materials=False,import_textures=False,import_animations=False).items():options.set_editor_property(key,value)
        for key,value in dict(combine_meshes=True,auto_generate_collision=False,generate_lightmap_u_vs=False,
                              convert_scene=True,convert_scene_unit=True,
                              vertex_color_import_option=unreal.VertexColorImportOption.REPLACE).items():options.static_mesh_import_data.set_editor_property(key,value)
        task.set_editor_property('options',options)
        TOOLS.import_asset_tasks([task])
        mesh=unreal.load_asset(DEST+'/Meshes/'+name)
        assert isinstance(mesh,unreal.StaticMesh),name
        agg=mesh.get_editor_property('body_setup').get_editor_property('agg_geom')
        shapes=sum(len(agg.get_editor_property(field)) for field in ['box_elems','convex_elems'])
        assert shapes>0,(name,'Missing authored collision')
        REPORT[name]=dict(collision_shapes=shapes,triangles=mesh.get_num_triangles(0))

GROUPS={}
def instance(mesh_name,location,scale=1.,yaw=0.,folder='장식'):
    scale=(scale,scale,scale) if isinstance(scale,(float,int)) else scale
    transform=unreal.Transform(location=unreal.Vector(*location),rotation=unreal.Rotator(yaw=yaw),scale=unreal.Vector(*scale))
    GROUPS.setdefault(mesh_name,[]).append(transform)

def grounded(name,x,y,scale=1.,yaw=0.,folder='장식'):
    scalar=scale if isinstance(scale,(float,int)) else scale[2]
    z=-IMPORTED['meshes']['SM_PGFR_'+name]['min'][2]*scalar
    instance(name,(x,y,z),scale,yaw,folder)

def solid(name,mesh_name,location,scale=1.,yaw=0.,folder='전투 지형',visible=True):
    actor=ACTORS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*location),unreal.Rotator(yaw=yaw))
    actor.set_actor_label(name);actor.set_folder_path(folder)
    component=actor.static_mesh_component
    component.set_static_mesh(MESHES[mesh_name])
    component.set_collision_profile_name('BlockAll')
    component.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE)
    actor.set_actor_scale3d(unreal.Vector(*((scale,scale,scale) if isinstance(scale,(int,float)) else scale)))
    if not visible:
        component.set_visibility(False);component.set_hidden_in_game(True);component.set_cast_shadow(False)
    REPORT['placements'].append(dict(label=name,mesh=mesh_name,location_cm=list(location),scale=scale,collidable=True,visible=visible))
    return actor

try:
    import_terrain()
    main=base_material('M_PGFR_Atlas',(.84,.90,.87))
    glow=base_material('M_PGFR_BlueLight',(.0,.36,1.),(.001,.10,.42))
    ground=ground_material()
    MESHES={}
    for name in IMPORTED['meshes']:
        mesh=unreal.load_asset(DEST+'/Meshes/'+name)
        for index in range(len(mesh.get_editor_property('static_materials'))):mesh.set_material(index,main if index==0 else glow)
        save(mesh);MESHES[name.removeprefix('SM_PGFR_')]=mesh
    for name in ['Ground','Boundary']:
        mesh=unreal.load_asset(DEST+'/Meshes/SM_PGFR_'+name);mesh.set_material(0,ground);save(mesh);MESHES[name]=mesh
    existing=ROOT/'Content/Maps/L_PG_ForestRuins.umap'
    if existing.exists():
        backup=OUT/'Backups'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        backup.mkdir(parents=True);shutil.copy2(existing,backup/existing.name)
        REPORT['backup']=str(backup)
    world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    mode=unreal.load_class(None,CONFIG['game_mode']);assert mode
    world.get_world_settings().set_editor_property('default_game_mode',mode)
    solid('연속 지면','Ground',(0,0,0),folder='지형')
    solid('석재 외곽 경계 충돌','Boundary',(0,0,0),folder='지형',visible=False)

    import sys
    sys.path.insert(0,str(KIT))
    import DetailedForestLayout
    DetailedForestLayout.populate(globals())

    start=ACTORS.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(*CONFIG['player_start_cm']))
    start.set_actor_label('플레이어 시작');start.set_folder_path('게임 플레이')
    nav=ACTORS.spawn_actor_from_class(unreal.NavMeshBoundsVolume,unreal.Vector(0,0,180))
    nav.set_actor_label('전투장 내비게이션');nav.set_folder_path('게임 플레이')
    origin,extent=nav.get_actor_bounds(False)
    assert min(extent.x,extent.y,extent.z)>0,('Empty navigation brush',vec(extent))
    target=CONFIG['navigation_half_extent_cm']
    nav.set_actor_scale3d(unreal.Vector(*(target[i]/vec(extent)[i] for i in range(3))))
    navmeshes=[a for a in ACTORS.get_all_level_actors() if isinstance(a,unreal.RecastNavMesh)]
    if not navmeshes:
        recast=unreal.PGEditorProbeTools.ensure_editor_navigation(world,nav)
        assert isinstance(recast,unreal.RecastNavMesh),'Could not initialize editor navigation data'
        navmeshes=[recast]
    for recast in navmeshes:
        recast.set_actor_label('전투장 경로 데이터');recast.set_folder_path('게임 플레이')
        recast.set_editor_property('runtime_generation',unreal.RuntimeGenerationType.DYNAMIC)
        recast.set_editor_property('force_rebuild_on_load',True)
    REPORT['navigation']=dict(bounds=[vec(v) for v in nav.get_actor_bounds(False)],dynamic=True)

    pp=ACTORS.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
    pp.set_actor_label('전투 가독성 노출');pp.set_folder_path('조명');pp.set_editor_property('unbound',True)
    settings=unreal.PostProcessSettings()
    for name,value in dict(override_auto_exposure_method=True,auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
        override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False,
        override_auto_exposure_bias=True,auto_exposure_bias=0.,override_motion_blur_amount=True,motion_blur_amount=0.,
        override_bloom_intensity=True,bloom_intensity=.12,
        override_local_exposure_highlight_contrast_scale=True,local_exposure_highlight_contrast_scale=1.,
        override_local_exposure_shadow_contrast_scale=True,local_exposure_shadow_contrast_scale=1.).items():settings.set_editor_property(name,value)
    pp.set_editor_property('settings',settings)
    key=ACTORS.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,1500),unreal.Rotator(*CONFIG['key_light_rotation']))
    key.set_actor_label('숲의 주광원');key.set_folder_path('조명')
    key.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    key.light_component.set_intensity(CONFIG['key_light_intensity'])
    key.light_component.set_editor_property('light_source_angle',7.)
    key.light_component.set_light_color(unreal.LinearColor(1.,.90,.72))
    fill=ACTORS.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,1600),unreal.Rotator(pitch=-65,yaw=120))
    fill.set_actor_label('숲의 보조광');fill.set_folder_path('조명')
    fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    fill.light_component.set_intensity(CONFIG['fill_light_intensity']);fill.light_component.set_cast_shadows(False)
    fill.light_component.set_light_color(unreal.LinearColor(.60,.78,1.))

    DetailedForestLayout.atmosphere(globals())

    for name,transforms in GROUPS.items():
        foliage=own('FT_PGFR_'+name,unreal.FoliageType_InstancedStaticMesh,unreal.FoliageType_InstancedStaticMeshFactory(),'Foliage')
        foliage.set_editor_property('mesh',MESHES[name])
        foliage.set_editor_property('mobility',unreal.ComponentMobility.STATIC)
        foliage.set_editor_property('cast_shadow',not any(word in name for word in ['Grass','Tile','FlatStone','Iris','Flower','Leaves','CirclePlant']))
        foliage.set_editor_property('collision_with_world',False)
        foliage.set_editor_property('cull_distance',unreal.Int32Interval(min=6500 if 'Grass' in name else 12000,max=8500 if 'Grass' in name else 16000))
        body=foliage.get_editor_property('body_instance')
        body.set_editor_property('collision_profile_name','NoCollision')
        body.set_editor_property('collision_enabled',unreal.CollisionEnabled.NO_COLLISION)
        foliage.set_editor_property('body_instance',body)
        save(foliage)
        unreal.InstancedFoliageActor.add_instances(world,foliage,transforms)
        REPORT['instances'][name]=len(transforms)
    REPORT['total_instances']=sum(REPORT['instances'].values())
    assert REPORT['total_instances']>600
    actual={}
    for actor in ACTORS.get_all_level_actors():
        if isinstance(actor,unreal.InstancedFoliageActor):
            actor.set_actor_label('석재 · 식생 인스턴스');actor.set_folder_path('환경 인스턴스')
            for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
                name=comp.get_editor_property('static_mesh').get_name().removeprefix('SM_PGFR_')
                actual[name]=actual.get(name,0)+comp.get_instance_count()
    assert actual==REPORT['instances'],('Missing foliage instances',actual)
    REPORT['actor_count']=len(ACTORS.get_all_level_actors())
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP),MAP
    for relative,expected in REPORT['protected'].items():
        assert hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==expected,relative
    REPORT['status']='PASS';unreal.log('PGForestRuins BUILD PASS')
except BaseException as error:
    REPORT.update(status='FAIL',error=str(error),traceback=traceback.format_exc())
    unreal.log_error(REPORT['traceback']);raise
finally:
    (OUT/'build.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
