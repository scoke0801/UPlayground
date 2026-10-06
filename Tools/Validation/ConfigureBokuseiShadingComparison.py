"""Create cumulative Bokusei shading stages and a playable free-camera fixture."""
import hashlib
import json
import shutil
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/Art/ToonTest/BokuseiShadingComparison'
MAP = '/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_ShadingComparison'
OUT = ROOT/'Saved/BokuseiShadingComparison'
RUN = OUT/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
RUN.mkdir(parents=True)
LIB = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
REPORT = dict(schema=2, status='RUNNING', map=MAP, run=str(RUN), slots=[], stages=[], protected={})


def package_file(path):
    return ROOT/'Content'/(path.split('.')[0].removeprefix('/Game/')+'.uasset')


def protect(asset):
    if not asset.get_path_name().startswith('/Game/'):
        return
    path = package_file(asset.get_path_name())
    REPORT['protected'][str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()


def save(asset):
    assert asset.get_path_name().startswith(DEST+'/'), 'Unexpected write: '+asset.get_path_name()
    assert EAL.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def own(name, cls, factory, folder='Materials'):
    path = DEST+'/'+folder+'/'+name
    asset = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(name, DEST+'/'+folder, cls, factory)
    assert asset, path
    return asset


def expr(material, cls, x=0, y=0):
    return LIB.create_material_expression(material, cls, x, y)


def scalar(material, name, value, x=0, y=0):
    node = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    return node


def vector(material, name, color, x=0, y=0):
    node = expr(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', unreal.LinearColor(*color))
    return node


def connect(source, output, target, pin):
    assert LIB.connect_material_expressions(source, output, target, pin)


def output(source, pin, prop):
    assert LIB.connect_material_property(source, pin, prop)


def master(name, translucent=False):
    material = own(name, unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(material)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT if translucent else unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property('two_sided', True)
    material.set_editor_property('used_with_skeletal_mesh', True)
    if translucent:
        material.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    texture = expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, -200)
    texture.set_editor_property('parameter_name', 'BaseTexture')
    texture.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
    tint = vector(material, 'BaseTint', (1, 1, 1, 1), -700, 0)
    color = expr(material, unreal.MaterialExpressionMultiply, -300, -100)
    connect(texture, 'RGB', color, 'A')
    connect(tint, 'RGB', color, 'B')
    output(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    # Match the current toon master's surface defaults. Remove analytic bands,
    # rim/specular art terms, emissive fill, flattened face normals and outlines.
    output(scalar(material, 'SurfaceRoughness', .85, -300, 200), '', unreal.MaterialProperty.MP_ROUGHNESS)
    output(scalar(material, 'SurfaceSpecular', 0, -300, 300), '', unreal.MaterialProperty.MP_SPECULAR)
    mask = expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -1000, 400)
    mask.set_editor_property('parameter_name', 'OpacityTexture')
    mask.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
    inputs = [(texture, 'A', 'BaseAlpha'), (mask, 'R', 'Mask')]
    for i, (name, default) in enumerate([('UseBaseAlpha', 1), ('AlphaMaskMode', 0), ('AlphaMaskScale', 1),
                                        ('AlphaMaskValue', 0), ('MainOpacity', 1), ('OpacityCutoff', .333)]):
        inputs.append((scalar(material, name, default, -1000, 600+i*90), '', name))
    alpha = expr(material, unreal.MaterialExpressionCustom, -300, 500)
    pins = []
    for _, _, name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    alpha.set_editor_property('inputs', pins)
    alpha.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    code = '''
float mask = saturate(Mask * AlphaMaskScale + AlphaMaskValue);
float a = lerp(1.0, BaseAlpha, saturate(UseBaseAlpha));
if (AlphaMaskMode >= .5 && AlphaMaskMode < 1.5) a = mask;
else if (AlphaMaskMode >= 1.5 && AlphaMaskMode < 2.5) a *= mask;
else if (AlphaMaskMode >= 2.5 && AlphaMaskMode < 3.5) a = saturate(a + mask);
else if (AlphaMaskMode >= 3.5) a = saturate(a - mask);
a *= MainOpacity;
'''
    alpha.set_editor_property('code', code+('return saturate(a);' if translucent else 'return step(OpacityCutoff, a);'))
    for source, pin, name in inputs:
        connect(source, pin, alpha, name)
    output(alpha, '', unreal.MaterialProperty.MP_OPACITY if translucent else unreal.MaterialProperty.MP_OPACITY_MASK)
    LIB.recompile_material(material)
    save(material)
    return material


def solid(name, color, lit=True):
    material = own(name, unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(material)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT if lit else unreal.MaterialShadingModel.MSM_UNLIT)
    output(vector(material, 'Color', (*color, 1)), 'RGB', unreal.MaterialProperty.MP_BASE_COLOR if lit else unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if lit:
        output(scalar(material, 'Roughness', 1, 0, 100), '', unreal.MaterialProperty.MP_ROUGHNESS)
    LIB.recompile_material(material)
    save(material)
    return material


def label_material(name):
    source = ROOT/'Tools/Art/ToonTest/BokuseiShadingComparison'/(name+'.png')
    assert source.is_file(), 'Run BuildLabels.ps1 first'
    texture_name = 'T_PGComparison_'+name
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = str(source), DEST+'/Labels', texture_name
    task.automated, task.replace_existing, task.save = True, True, True
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(DEST+'/Labels/'+texture_name)
    assert texture
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('never_stream', True)
    save(texture)
    material = own('M_PGComparison_'+name, unreal.Material, unreal.MaterialFactoryNew(), 'Labels')
    LIB.delete_all_material_expressions(material)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('two_sided', True)
    node = expr(material, unreal.MaterialExpressionTextureSample)
    node.set_editor_property('texture', texture)
    output(node, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    output(node, 'A', unreal.MaterialProperty.MP_OPACITY)
    LIB.recompile_material(material)
    save(material)
    return material


def toon_variant(source, slot, basic=False):
    name = ('MI_PGCel_' if basic else 'MI_PGPartShade_') + slot
    mi = own(name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    # Inherit current source values on every regeneration; only disable the
    # subsequent steps. No duplicated stale source overrides on reruns.
    mi.set_editor_property('scalar_parameter_values', [])
    mi.set_editor_property('vector_parameter_values', [])
    mi.set_editor_property('texture_parameter_values', [])
    LIB.set_material_instance_parent(mi, source)
    scalars = dict(RimStrength=0., SpecularStrength=0.)
    if basic:
        cloth = json.loads((ROOT/'Tools/Art/ToonTest/shading_profiles.json').read_text(encoding='utf-8'))['profiles']['cloth']
        scalars.update({key: cloth['scalars'][key] for key in
                        ['DiffuseWrap', 'ShadowSoftness', 'LightSoftness', 'BandAA', 'ShadeStrength']})
        for key in ['ShadowTint', 'MidTint', 'LightTint']:
            LIB.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(*cloth['vectors'][key]))
    for key, value in scalars.items():
        LIB.set_material_instance_scalar_parameter_value(mi, key, value)
        assert abs(LIB.get_material_instance_scalar_parameter_value(mi, key)-value) < 1e-6, key
    LIB.update_material_instance(mi)
    save(mi)
    return mi


def main():
    # Rebuild only the comparison outputs, with package backups on reruns.
    previous = ROOT/'Content/Art/ToonTest/BokuseiShadingComparison'
    if previous.exists():
        shutil.copytree(previous, RUN/'backup/assets')
    map_file = ROOT/'Content/Art/ToonTest/Maps/L_PGToon_Bokusei_ShadingComparison.umap'
    if map_file.exists():
        shutil.copy2(map_file, RUN/'backup/previous_map.umap')
    appearance = unreal.load_asset('/Game/DataCenter/Characters/DA_Bokusei')
    assert appearance
    mesh = appearance.get_editor_property('mesh')
    idle = unreal.load_asset('/Game/Art/ToonTest/Bokusei/Animation/PGBokusei_AS_Anime_KC_Idle')
    outline = unreal.load_asset('/Game/Art/ToonTest/Advanced/Materials/M_PGToonScreenOutline')
    assert mesh and idle and outline
    for asset in (appearance, mesh, idle, outline):
        protect(asset)
    originals = [slot.material_interface for slot in mesh.get_editor_property('materials')]
    for mi in originals:
        protect(mi)
        protect(mi.get_editor_property('parent'))
    masters = [master('M_PGComparison_DefaultLit'), master('M_PGComparison_DefaultLitTransparent', True)]
    lit_materials = []
    for slot in mesh.get_editor_property('materials'):
        source = slot.material_interface
        transparent = source.get_editor_property('parent').get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT
        target = DEST+'/Materials/MI_PGLit_'+str(slot.material_slot_name)
        mi = unreal.load_asset(target) if EAL.does_asset_exist(target) else EAL.duplicate_asset(source.get_path_name(), target)
        assert mi
        LIB.set_material_instance_parent(mi, masters[int(transparent)])
        # Copy readback, including inherited values, to keep alpha/color identical.
        for name in ['BaseTexture', 'OpacityTexture']:
            texture = LIB.get_material_instance_texture_parameter_value(source, name)
            if texture:
                LIB.set_material_instance_texture_parameter_value(mi, name, texture)
                protect(texture)
        LIB.set_material_instance_vector_parameter_value(mi, 'BaseTint', LIB.get_material_instance_vector_parameter_value(source, 'BaseTint'))
        for name in ['UseBaseAlpha', 'AlphaMaskMode', 'AlphaMaskScale', 'AlphaMaskValue', 'MainOpacity', 'OpacityCutoff']:
            LIB.set_material_instance_scalar_parameter_value(mi, name, LIB.get_material_instance_scalar_parameter_value(source, name))
        LIB.update_material_instance(mi)
        save(mi)
        lit_materials.append(mi)
        REPORT['slots'].append(dict(name=str(slot.material_slot_name), toon=source.get_path_name(), lit=mi.get_path_name(), translucent=transparent))
    stage = solid('M_PGComparison_Stage', (.11, .13, .16))
    backdrop = solid('M_PGComparison_Backdrop', (.016, .021, .032), False)
    definitions = [
        ('Lit', '1 · 일반 조명', lit_materials),
        ('Cel', '2 · 셀 명암', [toon_variant(mi, str(slot.material_slot_name), True) for mi, slot in zip(originals, mesh.get_editor_property('materials'))]),
        ('Parts', '3 · 부위별 명암', [toon_variant(mi, str(slot.material_slot_name)) for mi, slot in zip(originals, mesh.get_editor_property('materials'))]),
        ('Rim', '4 · 림·하이라이트', originals),
        ('Toon', '5 · 외곽선 · 완성', originals),
    ]
    labels = {name: label_material(name) for name in ['Title']+[d[0] for d in definitions]}
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.GameModeBase)
    mode = own('BP_PGShadingComparisonGameMode', unreal.Blueprint, factory, 'Blueprints')
    defaults = unreal.get_default_object(mode.generated_class())
    defaults.set_editor_property('default_pawn_class', unreal.PGShadingComparisonPawn)
    defaults.set_editor_property('hud_class', None)
    unreal.BlueprintEditorLibrary.compile_blueprint(mode)
    save(mode)
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    world.get_world_settings().set_editor_property('default_game_mode', mode.generated_class())
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    def prop(name, shape, location, scale, material, rotation=unreal.Rotator()):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), rotation)
        actor.set_actor_label(name)
        actor.set_folder_path('비교 무대')
        c = actor.static_mesh_component
        c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/'+shape))
        c.set_material(0, material)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        return actor

    prop('중성 회색 바닥', 'Cube', (0, 0, -12), (26, 20, .12), stage)
    prop('중성 배경', 'Cube', (0, -160, 220), (26, .08, 7), backdrop)
    pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
    pp.set_actor_label('공통 노출 · 툰 외곽선')
    pp.set_editor_property('unbound', True)
    settings = unreal.PostProcessSettings()
    for name, value in dict(override_auto_exposure_method=True, auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
                            override_auto_exposure_apply_physical_camera_exposure=True, auto_exposure_apply_physical_camera_exposure=False,
                            override_auto_exposure_bias=True, auto_exposure_bias=0., override_bloom_intensity=True, bloom_intensity=0.,
                            override_motion_blur_amount=True, motion_blur_amount=0.,
                            override_local_exposure_highlight_contrast_scale=True, local_exposure_highlight_contrast_scale=1.,
                            override_local_exposure_shadow_contrast_scale=True, local_exposure_shadow_contrast_scale=1.).items():
        settings.set_editor_property(name, value)
    pp.set_editor_property('settings', settings)
    pp.add_or_update_blendable(outline, 1.)
    key = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 200, 400), unreal.Rotator(pitch=-55, yaw=-52, roll=0))
    key.set_actor_label('주광원 · 모든 단계 공통')
    key.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    key.light_component.set_intensity(3.2)
    key.light_component.set_editor_property('light_source_angle', 3.)
    fill = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, -100, 300), unreal.Rotator(pitch=-35, yaw=70, roll=0))
    fill.set_actor_label('보조광 · 모든 단계 공통')
    fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    fill.light_component.set_intensity(.6)
    fill.light_component.set_cast_shadows(False)
    model_components = []
    for index, (stage_id, name, materials) in enumerate(definitions):
        position = (index-2)*220
        actor = actors.spawn_actor_from_class(unreal.PGToonPreviewActor if index else unreal.SkeletalMeshActor, unreal.Vector(position, 0, 0))
        actor.set_actor_label(name)
        actor.set_folder_path('비교 모델')
        actor.set_editor_property('tags', ['PGShadingComparisonModel', 'PGShadingStage'+str(index)])
        component = actor.skeletal_mesh_component
        model_components.append(component)
        component.set_skeletal_mesh_asset(mesh)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property('cast_shadow', True)
        component.set_render_custom_depth(index == 4)
        component.set_custom_depth_stencil_value(73 if index == 4 else 0)
        component.set_forced_lod(1)
        for i, material in enumerate(materials):
            component.set_material(i, material)
        data = unreal.SingleAnimationPlayData()
        data.anim_to_play, data.saved_looping, data.saved_playing = idle, True, True
        data.saved_position, data.saved_play_rate = 1.25, 1.
        component.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        component.set_editor_property('animation_data', data)
        component.set_animation(idle)
        component.set_position(1.25, False)
        component.set_update_animation_in_editor(True)
        component.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        if index:
            p = actor.toon_presentation
            p.set_editor_property('key_light', key)
            for name in ['head_bone', 'head_forward_axis', 'head_right_axis']:
                p.set_editor_property(name, appearance.get_editor_property(name))
        # Same geometry, scale and placement on equal pedestals.
        prop('받침대 '+str(index+1), 'Cylinder', (position, 0, -3), (1.45, 1.45, .06), stage)
        sign = prop('안내 · '+name, 'Plane', (position, 0, 195), (2., .4, 1), labels[stage_id], unreal.Rotator(roll=90))
        sign.static_mesh_component.set_cast_shadow(False)
        REPORT['stages'].append(dict(index=index, id=stage_id, label=name, position=position,
                                     materials=[mi.get_path_name() for mi in materials], outline=index == 4))
    for component in model_components[1:]:
        component.set_leader_pose_component(model_components[0])
    title = prop('안내 · bOKUSEI 셰이딩 비교', 'Plane', (0, 0, 270), (6.8, .6375, 1), labels['Title'], unreal.Rotator(roll=90))
    title.static_mesh_component.set_cast_shadow(False)
    for name, location, target, fov, active in [
            ('카메라 · 정면 비교', (0, 1650, 330), (0, 0, 120), 45, True),
            ('카메라 · 쿼터뷰 비교', (390, 1600, 1000), (0, 0, 100), 45, False),
            ('카메라 · 얼굴 비교', (330, 340, 147), (330, 0, 139), 45, False)]:
        loc, aim = unreal.Vector(*location), unreal.Vector(*target)
        rot = unreal.MathLibrary.find_look_at_rotation(loc, aim)
        camera = actors.spawn_actor_from_class(unreal.CameraActor, loc, rot)
        camera.set_actor_label(name)
        camera.set_folder_path('비교 카메라')
        camera.camera_component.set_editor_property('field_of_view', fov)
        if active:
            camera.set_editor_property('tags', ['PGShadingOverview'])
            level.set_level_viewport_camera_info(loc, rot, 'None')
            level.set_level_viewport_fov(fov, 'None')
            start = actors.spawn_actor_from_class(unreal.PlayerStart, loc, rot)
            start.set_actor_label('자유 카메라 시작 위치')
            start.set_folder_path('비교 카메라')
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest() == expected for p, expected in REPORT['protected'].items()), 'Source asset changed'
    REPORT.update(status='PASS', source_mesh=mesh.get_path_name(), animation=idle.get_path_name(),
                  game_mode=mode.get_path_name(), toon_basis='Current DA_Bokusei mesh-slot Unlit toon materials + stencil 73 screen outline',
                  original_assets_unchanged=True, free_camera='/Script/PGActor.PGShadingComparisonPawn')


try:
    main()
except Exception:
    REPORT.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    payload = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (OUT/'configure.json').write_text(payload, encoding='utf-8')
    (RUN/'configure.json').write_text(payload, encoding='utf-8')
if REPORT['status'] != 'PASS':
    raise RuntimeError('Bokusei shading comparison creation failed')
