"""Create cumulative Bokusei shading stages and a playable free-camera fixture."""
import hashlib
import json
import math
import shutil
import sys
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
REPORT = dict(schema=4, status='RUNNING', map=MAP, run=str(RUN), slots=[], stages=[], protected={})
HAIR_SETTINGS_PATH = ROOT/'Tools/Art/ToonTest/BokuseiShadingComparison/hair_shadow_settings.json'
HAIR_SETTINGS = json.loads(HAIR_SETTINGS_PATH.read_text(encoding='utf-8'))
assert HAIR_SETTINGS['schema'] == 1 and isinstance(HAIR_SETTINGS['two_sided'], bool)
for name, minimum, maximum in [('opacity_cutoff', .01, 1.), ('inset_cm', -1., 0.),
                                ('light_source_angle_degrees', 0., 30.)]:
    value = HAIR_SETTINGS[name]
    assert isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) and minimum <= value <= maximum, name


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


def shadow_variants(originals, slots):
    # Reuse the established world-lit toon graph, with face normal/anisotropy
    # extensions disabled so this stage keeps the original analytic art terms.
    sys.path.insert(0, str(ROOT/'Tools/Validation'))
    import ConfigureToonCharacterTest as shared
    shared.MASTER_DEST = DEST+'/Materials'
    shadow_masters = [shared.build_toon_master('M_PGComparison_ToonShadow', True, False, True),
                      shared.build_toon_master('M_PGComparison_ToonShadowTransparent', True, True, True)]
    result = []
    for source, slot in zip(originals, slots):
        transparent = source.get_editor_property('parent').get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT
        mi = own('MI_PGShadow_'+str(slot.material_slot_name), unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        for kind in ['scalar', 'vector', 'texture']:
            mi.set_editor_property(kind+'_parameter_values', [])
        LIB.set_material_instance_parent(mi, shadow_masters[int(transparent)])
        for kind in ['scalar', 'vector', 'texture']:
            names = getattr(LIB, 'get_'+kind+'_parameter_names')(source)
            for name in names:
                value = getattr(LIB, 'get_material_instance_'+kind+'_parameter_value')(source, name)
                if value is not None:
                    getattr(LIB, 'set_material_instance_'+kind+'_parameter_value')(mi, name, value)
        for name, value in dict(WorldLightingInfluence=.65, FaceShading=0., HairAnisotropy=0.,
                                ShadowCast=0. if 'face' in str(slot.material_slot_name).lower() else 1.).items():
            LIB.set_material_instance_scalar_parameter_value(mi, name, value)
        LIB.update_material_instance(mi)
        save(mi)
        result.append(mi)
    return result


def hair_shadow_materials(originals, slots):
    # The visible hair stays translucent. Only these masked proxy slots cast,
    # using the same alpha textures; every non-hair slot is completely clipped.
    masked_master = master('M_PGComparison_HairShadow')
    masked_master.set_editor_property('two_sided', HAIR_SETTINGS['two_sided'])
    # Inset the duplicate by 0.8 mm along its vertex normal to avoid coplanar
    # shadow acne on the visible hair while retaining the face shadow silhouette.
    normal = expr(masked_master, unreal.MaterialExpressionVertexNormalWS, -700, 1500)
    inset = expr(masked_master, unreal.MaterialExpressionMultiply, -300, 1500)
    connect(normal, '', inset, 'A')
    connect(scalar(masked_master, 'ShadowInset', HAIR_SETTINGS['inset_cm'], -700, 1650), '', inset, 'B')
    output(inset, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    LIB.recompile_material(masked_master)
    save(masked_master)
    invisible = own('M_PGComparison_NoShadow', unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(invisible)
    invisible.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    invisible.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    invisible.set_editor_property('used_with_skeletal_mesh', True)
    output(scalar(invisible, 'Opacity', 0.), '', unreal.MaterialProperty.MP_OPACITY_MASK)
    LIB.recompile_material(invisible)
    save(invisible)
    materials, hair_slots = [], []
    for index, (source, slot) in enumerate(zip(originals, slots)):
        if 'hair' not in str(slot.material_slot_name).lower():
            materials.append(invisible)
            continue
        mi = own('MI_PGHairShadow_'+str(slot.material_slot_name), unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        for kind in ['scalar', 'vector', 'texture']:
            mi.set_editor_property(kind+'_parameter_values', [])
        LIB.set_material_instance_parent(mi, masked_master)
        for name in ['BaseTexture', 'OpacityTexture']:
            texture = LIB.get_material_instance_texture_parameter_value(source, name)
            if texture:
                LIB.set_material_instance_texture_parameter_value(mi, name, texture)
        LIB.set_material_instance_vector_parameter_value(mi, 'BaseTint', LIB.get_material_instance_vector_parameter_value(source, 'BaseTint'))
        alpha = {}
        for name in ['UseBaseAlpha', 'AlphaMaskMode', 'AlphaMaskScale', 'AlphaMaskValue', 'MainOpacity', 'OpacityCutoff']:
            alpha[name] = LIB.get_material_instance_scalar_parameter_value(source, name)
            LIB.set_material_instance_scalar_parameter_value(mi, name, alpha[name])
        # Translucent source cutoffs are 0/.001. A masked caster needs a real
        # threshold, otherwise the padded hair cards become solid shadow slabs.
        alpha['OpacityCutoff'] = HAIR_SETTINGS['opacity_cutoff']
        LIB.set_material_instance_scalar_parameter_value(mi, 'OpacityCutoff', alpha['OpacityCutoff'])
        LIB.set_material_instance_scalar_parameter_value(mi, 'ShadowInset', HAIR_SETTINGS['inset_cm'])
        LIB.update_material_instance(mi)
        save(mi)
        materials.append(mi)
        hair_slots.append(dict(index=index, name=str(slot.material_slot_name), material=mi.get_path_name(), alpha=alpha))
    assert len(hair_slots) == 2, hair_slots
    REPORT['hair_shadow'] = dict(stage=6, slots=hair_slots, materials=[m.get_path_name() for m in materials],
                                 toggle_key='J', default_enabled=True, synchronized_pose=True,
                                 shadow_inset_cm=HAIR_SETTINGS['inset_cm'], settings=HAIR_SETTINGS,
                                 settings_source=str(HAIR_SETTINGS_PATH.relative_to(ROOT)),
                                 settings_sha256=hashlib.sha256(HAIR_SETTINGS_PATH.read_bytes()).hexdigest())
    return materials


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
    sdf_path = ROOT/'Saved/BokuseiFaceSDF/configure.json'
    sdf_data = json.loads(sdf_path.read_text(encoding='utf-8')) if sdf_path.exists() else None
    if sdf_data:
        assert sdf_data['status'] == 'PASS'
        for mi in originals: protect(mi)
        face_index = next(i for i,s in enumerate(mesh.get_editor_property('materials')) if str(s.material_slot_name) == 'Mat_Bokusei_Face')
        originals[face_index] = unreal.load_asset(sdf_data['baseline_face'])
        assert originals[face_index]
        REPORT['face_sdf'] = sdf_data
        REPORT['schema'] = 5
    for mi in originals:
        protect(mi)
        protect(mi.get_editor_property('parent'))
    masters = [master('M_PGComparison_DefaultLit'), master('M_PGComparison_DefaultLitTransparent', True)]
    lit_materials = []
    for slot_index,slot in enumerate(mesh.get_editor_property('materials')):
        source = originals[slot_index]
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
        ('Toon', '5 · 외곽선 · 기존 툰', originals),
        ('Shadow', '6 · 월드 그림자', shadow_variants(originals, mesh.get_editor_property('materials'))),
    ]
    if sdf_data:
        sdf_materials = list(originals)
        sdf_materials[face_index] = unreal.load_asset(sdf_data['sdf_face'])
        sdf_world = list(definitions[5][2])
        sdf_world[face_index] = unreal.load_asset(sdf_data['world_face'])
        assert sdf_materials[face_index] and sdf_world[face_index]
        for mi in [sdf_materials[face_index],sdf_world[face_index]]:
            protect(mi)
            protect(mi.get_editor_property('parent'))
        definitions += [('FaceSDF','7 · 얼굴 SDF',sdf_materials),('FaceSDFWorld','8 · 얼굴 SDF · 월드 그림자',sdf_world)]
    hair_materials = hair_shadow_materials(originals, mesh.get_editor_property('materials'))
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
    key.light_component.set_editor_property('light_source_angle', HAIR_SETTINGS['light_source_angle_degrees'])
    fill = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, -100, 300), unreal.Rotator(pitch=-35, yaw=70, roll=0))
    fill.set_actor_label('보조광 · 모든 단계 공통')
    fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    fill.light_component.set_intensity(.6)
    fill.light_component.set_cast_shadows(False)
    model_components = []
    for index, (stage_id, name, materials) in enumerate(definitions):
        position = (index-(len(definitions)-1)/2)*220
        actor = actors.spawn_actor_from_class(unreal.PGToonPreviewActor if index else unreal.SkeletalMeshActor, unreal.Vector(position, 0, 0))
        actor.set_actor_label(name)
        actor.set_folder_path('비교 모델')
        actor.set_editor_property('tags', ['PGShadingComparisonModel', 'PGShadingStage'+str(index)])
        component = actor.skeletal_mesh_component
        model_components.append(component)
        component.set_skeletal_mesh_asset(mesh)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property('cast_shadow', True)
        component.set_render_custom_depth(index >= 4)
        component.set_custom_depth_stencil_value(73 if index >= 4 else 0)
        component.set_forced_lod(1)
        for i, material in enumerate(materials):
            component.set_material(i, material)
        if index in [5,7]:
            proxy = actor.hair_shadow_proxy
            proxy.set_skeletal_mesh_asset(mesh)
            proxy.set_forced_lod(1)
            proxy.set_leader_pose_component(component)
            for i, material in enumerate(hair_materials):
                proxy.set_material(i, material)
            proxy.set_cast_shadow(True)
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
                                     materials=[mi.get_path_name() for mi in materials], outline=index >= 4))
        if index >= 4:
            # Keep H's occlusion readable under the shared soft area light.
            # Every receiving/reference stage uses the same 130x50x14 cm blocker.
            caster = prop('가림막 그림자 · '+str(index+1)+'단계', 'Cube', (position-39, 50, 230), (1.3, .5, .14), stage)
            caster.set_editor_property('tags', ['PGShadingShadowCaster'])
            caster.set_folder_path('그림자 비교')
            c = caster.static_mesh_component
            c.set_mobility(unreal.ComponentMobility.MOVABLE)
            c.set_editor_property('cast_hidden_shadow', True)
            c.set_cast_shadow(False)
            c.set_visibility(False)
    for component in model_components[1:]:
        component.set_leader_pose_component(model_components[0])
    title = prop('안내 · bOKUSEI 셰이딩 비교', 'Plane', (0, 0, 270), (6.8, .6375, 1), labels['Title'], unreal.Rotator(roll=90))
    title.static_mesh_component.set_cast_shadow(False)
    for name, location, target, fov, active in [
            ('카메라 · 정면 비교', (0, 1950, 380), (0, 0, 120), 45, True),
            ('카메라 · 쿼터뷰 비교', (390, 1900, 1100), (0, 0, 100), 45, False),
            ('카메라 · 얼굴 비교', (550 if sdf_data else 440, 340, 147), (550 if sdf_data else 440, 0, 139), 45, False)]:
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
    from BokuseiGuestModels import add_guests
    REPORT['guests'] = add_guests(actors, key, prop, label_material)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest() == expected for p, expected in REPORT['protected'].items()), 'Source asset changed'
    REPORT.update(status='PASS', source_mesh=mesh.get_path_name(), animation=idle.get_path_name(),
                  game_mode=mode.get_path_name(), toon_basis='Current DA_Bokusei mesh-slot Unlit toon materials + stencil 73 screen outline',
                  original_assets_unchanged=True, free_camera='/Script/PGActor.PGShadingComparisonPawn',
                  shadow=dict(stage=6, world_lighting_influence=.65, casters=len(definitions)-4, toggle_key='H', default_enabled=False))


if __name__ == '__main__':
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
