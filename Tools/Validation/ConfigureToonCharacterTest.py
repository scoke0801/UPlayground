"""Import one lilToon-authored Unity character and build an Unreal toon test setup.

Run with UnrealEditor-Cmd and PythonScriptPlugin. The script is intentionally
isolated under /Game/Art/ToonTest and is safe to rerun.
"""

import json
import re
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
MANIFEST_PATH = ROOT / "Tools/Art/ToonTest/Inori/manifest.json"
REPORT_DIR = ROOT / "Saved/ToonTest"
REPORT_PATH = REPORT_DIR / "import.json"
MASTER_DEST = "/Game/Art/ToonTest/Materials"

if __name__ == '__main__':
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text('{"status":"RUNNING"}', encoding="utf-8")

manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
source_root = Path(manifest["source_root"])
destination = manifest["destination"]
mesh_path = f'{destination}/{manifest["skeletal_mesh"]}'
tools = unreal.AssetToolsHelpers.get_asset_tools()
material_lib = unreal.MaterialEditingLibrary


def require_file(path: Path) -> None:
    if not path.is_file():
        raise RuntimeError(f"Required source file is missing: {path}")


def save(asset) -> None:
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {asset.get_path_name()}")


def recreate_material(name: str):
    path = f"{MASTER_DEST}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
    else:
        asset = tools.create_asset(
            name, MASTER_DEST, unreal.Material, unreal.MaterialFactoryNew()
        )
    if not asset:
        raise RuntimeError(f"Could not create material {path}")
    material_lib.delete_all_material_expressions(asset)
    return asset


def expression(material, cls, x=0, y=0):
    return material_lib.create_material_expression(material, cls, x, y)


def vector_parameter(material, name, value, x, y):
    node = expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(*value))
    return node


def scalar_parameter(material, name, value, x, y):
    node = expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def custom_input(name):
    value = unreal.CustomInput()
    value.set_editor_property("input_name", name)
    return value


def shading_profile(character, slot):
    config = json.loads((ROOT / 'Tools/Art/ToonTest/shading_profiles.json').read_text(encoding='utf-8'))
    name = config['slots'][character][slot]
    chain, seen = [], set()
    current = name
    while current:
        if current in seen:
            raise ValueError('Cyclic toon profile: ' + current)
        seen.add(current)
        row = config['profiles'][current]
        chain.append(row)
        current = row.get('inherits')
    result = {'name': name, 'scalars': {}, 'vectors': {}}
    for row in reversed(chain):
        for kind in ['scalars', 'vectors']:
            result[kind].update(row.get(kind, {}))
    return result


def apply_shading_profile(instance, character, slot):
    profile = shading_profile(character, slot)
    for name, value in profile['scalars'].items():
        # UE 5.8 can return False even when the override was written. Verify readback.
        material_lib.set_material_instance_scalar_parameter_value(instance, name, value)
        if abs(material_lib.get_material_instance_scalar_parameter_value(instance, name)-value) > 1e-5:
            raise RuntimeError('Toon scalar did not persist: ' + name)
    for name, value in profile['vectors'].items():
        material_lib.set_material_instance_vector_parameter_value(instance, name, unreal.LinearColor(*value))
        actual = material_lib.get_material_instance_vector_parameter_value(instance, name)
        if any(abs(getattr(actual, axis)-v) > 1e-5 for axis, v in zip(['r', 'g', 'b', 'a'], value)):
            raise RuntimeError('Toon vector did not persist: ' + name)
    material_lib.update_material_instance(instance)
    return profile


def build_toon_master(name="M_PGToonCharacter", extended_alpha=False, translucent=False, world_lit=False):
    material = recreate_material(name)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT if translucent else unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT if world_lit else unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_skeletal_mesh", True)
    material.set_editor_property("opacity_mask_clip_value", 0.333)

    texture = expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -1050, -260
    )
    texture.set_editor_property("parameter_name", "BaseTexture")
    texture.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
    tint = vector_parameter(material, "BaseTint", (1.0, 1.0, 1.0, 1.0), -1050, -80)
    normal = expression(material, unreal.MaterialExpressionVertexNormalWS, -1050, 100)
    geometric_normal = normal
    if world_lit:
        material.set_editor_property('tangent_space_normal', False)
        if translucent:
            material.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        face = scalar_parameter(material, 'FaceShading', 0, -2050, 0)
        forward = vector_parameter(material, 'HeadForwardWS', (0, 1, 0, 0), -2050, 100)
        right = vector_parameter(material, 'HeadRightWS', (1, 0, 0, 0), -2050, 200)
        face_normal = expression(material, unreal.MaterialExpressionCustom, -1800, 0)
        face_normal.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        face_normal.set_editor_property('inputs', [custom_input(p) for p in ['N', 'Forward', 'Right', 'Strength']])
        face_normal.set_editor_property('code', '''
float3 F = Forward * rsqrt(max(dot(Forward, Forward), 1e-6));
float3 R = Right - F * dot(Right, F);
R *= rsqrt(max(dot(R, R), 1e-6));
float3 U = cross(R, F);
float3 faceN = F + R * dot(N, R) * .35 + U * dot(N, U) * .12;
float3 result = lerp(N, normalize(faceN), saturate(Strength));
return result * rsqrt(max(dot(result, result), 1e-6));
'''.strip())
        for src, output, pin in [(normal, '', 'N'), (forward, 'RGB', 'Forward'), (right, 'RGB', 'Right'), (face, '', 'Strength')]:
            assert material_lib.connect_material_expressions(src, output, face_normal, pin)
        normal = face_normal
        # Keep geometric normals for engine lighting; the flattened art normal
        # controls only the analytic toon bands.
        assert material_lib.connect_material_property(geometric_normal, '', unreal.MaterialProperty.MP_NORMAL)
    camera = expression(material, unreal.MaterialExpressionCameraVectorWS, -1050, 190)
    light = vector_parameter(
        material, "LightDirection", (0.35, -0.45, -0.82, 0.0), -1050, 280
    )
    shadow = vector_parameter(
        material, "ShadowTint", (0.42, 0.48, 0.62, 1.0), -1050, 390
    )
    mid = vector_parameter(material, "MidTint", (0.76, 0.80, 0.88, 1.0), -1050, 500)
    light_tint = vector_parameter(
        material, "LightTint", (1.0, 0.98, 0.95, 1.0), -1050, 610
    )
    shadow_threshold = scalar_parameter(material, "ShadowThreshold", 0.36, -780, 390)
    light_threshold = scalar_parameter(material, "LightThreshold", 0.72, -780, 500)
    rim_color = vector_parameter(
        material, "RimColor", (0.35, 0.52, 1.0, 1.0), -780, 610
    )
    rim_strength = scalar_parameter(material, "RimStrength", 0.18, -780, 720)
    rim_power = scalar_parameter(material, "RimPower", 3.5, -780, 810)
    state_color = vector_parameter(
        material, "StateColor", (0.0, 0.0, 0.0, 1.0), -510, 610
    )
    state_glow = scalar_parameter(material, "StateGlow", 0.0, -510, 720)

    custom = expression(material, unreal.MaterialExpressionCustom, -230, 60)
    custom.set_editor_property(
        "code",
        (ROOT / 'Tools/Art/ToonTest/ToonShading.hlsl').read_text(encoding='utf-8'),
    )
    if world_lit:
        custom.set_editor_property('code', custom.get_editor_property('code').replace(
            'float spec = pow(saturate(dot(N, H)), max(SpecularPower, 1.0));', '''
float tangentH = dot(normalize(HairTangentWS + 1e-6), H);
float strandSpec = pow(sqrt(saturate(1.0 - tangentH * tangentH)), max(SpecularPower, 1.0));
float spec = lerp(pow(saturate(dot(N, H)), max(SpecularPower, 1.0)), strandSpec, saturate(HairAnisotropy));'''))
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    extra_scalars = {
        'ShadowSoftness': .025, 'LightSoftness': .035, 'BandAA': .75,
        'DiffuseWrap': .12, 'ShadeStrength': 1., 'RimThreshold': .38,
        'RimSoftness': .13, 'RimLightMask': .85, 'RimBaseBlend': .65,
        'SpecularStrength': 0., 'SpecularPower': 32., 'SpecularThreshold': .5,
        'SpecularSoftness': .075,
    }
    extra_nodes = [(scalar_parameter(material, key, value, -1550, 900+i*90), '', key)
                   for i, (key, value) in enumerate(extra_scalars.items())]
    extra_nodes += [(vector_parameter(material, 'SpecularTint', (.83, .85, 1., 1.), -1550, 2200), 'RGB', 'SpecularTint'),
                    (expression(material, unreal.MaterialExpressionTwoSidedSign, -1550, 2300), '', 'FaceSign')]
    if world_lit:
        extra_nodes += [(scalar_parameter(material, 'HairAnisotropy', 0, -1550, 2400), '', 'HairAnisotropy'),
                        (expression(material, unreal.MaterialExpressionVertexTangentWS, -1550, 2500), '', 'HairTangentWS')]
    custom.set_editor_property(
        "inputs",
        [
            custom_input("BaseColor"),
            custom_input("BaseTint"),
            custom_input("NormalWS"),
            custom_input("CameraVector"),
            custom_input("LightDirection"),
            custom_input("ShadowTint"),
            custom_input("MidTint"),
            custom_input("LightTint"),
            custom_input("ShadowThreshold"),
            custom_input("LightThreshold"),
            custom_input("RimColor"),
            custom_input("RimStrength"),
            custom_input("RimPower"),
            custom_input("StateColor"),
            custom_input("StateGlow"),
        ] + [custom_input(pin) for _, _, pin in extra_nodes],
    )
    for source, output_name, input_name in [
        (texture, "RGB", "BaseColor"),
        (tint, "RGB", "BaseTint"),
        (normal, "", "NormalWS"),
        (camera, "", "CameraVector"),
        (light, "RGB", "LightDirection"),
        (shadow, "RGB", "ShadowTint"),
        (mid, "RGB", "MidTint"),
        (light_tint, "RGB", "LightTint"),
        (shadow_threshold, "", "ShadowThreshold"),
        (light_threshold, "", "LightThreshold"),
        (rim_color, "RGB", "RimColor"),
        (rim_strength, "", "RimStrength"),
        (rim_power, "", "RimPower"),
        (state_color, "RGB", "StateColor"),
        (state_glow, "", "StateGlow"),
    ] + extra_nodes:
        if not material_lib.connect_material_expressions(source, output_name, custom, input_name):
            raise RuntimeError(f"Could not connect {input_name}")
    if not material_lib.connect_material_property(
        custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ):
        raise RuntimeError("Could not connect toon shading")
    if world_lit:
        # Hybrid art-directed bands with real UE direct/indirect lighting and shadows.
        # A small fill keeps shadowed anime features readable, adjustable down to zero.
        influence = scalar_parameter(material, 'WorldLightingInfluence', .88, 200, 250)
        for label, code, prop in [
            ('LitColor', 'return Color * saturate(Influence);', unreal.MaterialProperty.MP_BASE_COLOR),
            ('FillColor', 'return Color * (1.0 - saturate(Influence)) + StateColor * StateGlow * saturate(Influence);', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]:
            route = expression(material, unreal.MaterialExpressionCustom, 450, 150 if label == 'LitColor' else 350)
            route.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
            route.set_editor_property('code', code)
            route.set_editor_property('inputs', [custom_input(p) for p in ['Color', 'Influence', 'StateColor', 'StateGlow']])
            for src, output, pin in [(custom, '', 'Color'), (influence, '', 'Influence'), (state_color, 'RGB', 'StateColor'), (state_glow, '', 'StateGlow')]:
                assert material_lib.connect_material_expressions(src, output, route, pin)
            assert material_lib.connect_material_property(route, '', prop)
        for key, value, prop in [('SurfaceRoughness', .85, unreal.MaterialProperty.MP_ROUGHNESS), ('SurfaceSpecular', 0., unreal.MaterialProperty.MP_SPECULAR)]:
            node = scalar_parameter(material, key, value, 200, 500)
            assert material_lib.connect_material_property(node, '', prop)

    alpha_source, alpha_output = texture, "A"
    if extended_alpha:
        use_alpha = scalar_parameter(material, 'UseBaseAlpha', 1, -1550, -200)
        alpha_texture = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1550, -700)
        alpha_texture.set_editor_property("parameter_name", "OpacityTexture")
        alpha_texture.set_editor_property("texture", unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
        alpha_mode = scalar_parameter(material, "AlphaMaskMode", 0, -1550, -500)
        alpha_scale = scalar_parameter(material, "AlphaMaskScale", 1, -1550, -400)
        alpha_value = scalar_parameter(material, "AlphaMaskValue", 0, -1550, -300)
        alpha_source = expression(material, unreal.MaterialExpressionCustom, -1150, -700)
        alpha_source.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        alpha_source.set_editor_property("inputs", [custom_input(n) for n in ['BaseAlpha', 'Mask', 'Mode', 'Scale', 'Value', 'UseBaseAlpha']])
        alpha_source.set_editor_property("code", '''
float mask = saturate(Mask * Scale + Value);
BaseAlpha = lerp(1.0, BaseAlpha, saturate(UseBaseAlpha));
if (Mode < 0.5) return BaseAlpha;
if (Mode < 1.5) return mask;
if (Mode < 2.5) return BaseAlpha * mask;
if (Mode < 3.5) return saturate(BaseAlpha + mask);
return saturate(BaseAlpha - mask);
'''.strip())
        for src, output, pin in [(texture, 'A', 'BaseAlpha'), (alpha_texture, 'R', 'Mask'),
                                 (alpha_mode, '', 'Mode'), (alpha_scale, '', 'Scale'), (alpha_value, '', 'Value'), (use_alpha, '', 'UseBaseAlpha')]:
            assert material_lib.connect_material_expressions(src, output, alpha_source, pin)
        alpha_output = ''
        main_opacity = scalar_parameter(material, 'MainOpacity', 1, -1000, -650)
        scaled_alpha = expression(material, unreal.MaterialExpressionMultiply, -750, -650)
        assert material_lib.connect_material_expressions(alpha_source, alpha_output, scaled_alpha, 'A')
        assert material_lib.connect_material_expressions(main_opacity, '', scaled_alpha, 'B')
        alpha_source = scaled_alpha
    uv = expression(material, unreal.MaterialExpressionTextureCoordinate, -650, -210)
    dissolve = scalar_parameter(material, "DissolveAmount", 0.0, -650, -110)
    cutoff = scalar_parameter(material, "OpacityCutoff", 0.333, -650, -10)
    opacity = expression(material, unreal.MaterialExpressionCustom, -350, -220)
    opacity.set_editor_property(
        "code",
        """
float noise = frac(sin(dot(floor(UV * 180.0), float2(12.9898, 78.233))) * 43758.5453);
float alive = DissolveAmount <= 0.0 ? 1.0 : step(saturate(DissolveAmount), noise);
return step(OpacityCutoff, Alpha) * alive;
""".strip(),
    )
    opacity.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    if translucent:
        opacity.set_editor_property('code', '''
float noise = frac(sin(dot(floor(UV * 180.0), float2(12.9898, 78.233))) * 43758.5453);
float alive = DissolveAmount <= 0.0 ? 1.0 : step(saturate(DissolveAmount), noise);
return saturate(Alpha) * alive;
'''.strip())
    opacity.set_editor_property(
        "inputs",
        [
            custom_input("Alpha"),
            custom_input("UV"),
            custom_input("DissolveAmount"),
            custom_input("OpacityCutoff"),
        ],
    )
    for source, output_name, input_name in [
        (alpha_source, alpha_output, "Alpha"),
        (uv, "", "UV"),
        (dissolve, "", "DissolveAmount"),
        (cutoff, "", "OpacityCutoff"),
    ]:
        if not material_lib.connect_material_expressions(source, output_name, opacity, input_name):
            raise RuntimeError(f"Could not connect opacity {input_name}")
    if world_lit and not translucent:
        # Facial geometry may omit its tiny cast shadows while still receiving
        # hair/world shadows. CustomDepth remains intact for the silhouette pass.
        cast = scalar_parameter(material, 'ShadowCast', 1., -350, -400)
        shadow_opacity = expression(material, unreal.MaterialExpressionCustom, -50, -220)
        shadow_opacity.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        shadow_opacity.set_editor_property('inputs', [custom_input(p) for p in ['Opacity', 'ShadowCast']])
        shadow_opacity.set_editor_property('code', 'return Opacity * lerp(1.0, saturate(ShadowCast), IsShadowDepthShader());')
        assert material_lib.connect_material_expressions(opacity, '', shadow_opacity, 'Opacity')
        assert material_lib.connect_material_expressions(cast, '', shadow_opacity, 'ShadowCast')
        opacity = shadow_opacity
    if not material_lib.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY if translucent else unreal.MaterialProperty.MP_OPACITY_MASK
    ):
        raise RuntimeError("Could not connect opacity mask")

    material_lib.recompile_material(material)
    save(material)
    return material


def connect_outline_offset(material, normal, width):
    """Bounded distance scaling: thin portraits, readable quarter-view silhouettes."""
    position = expression(material, unreal.MaterialExpressionWorldPosition, -1600, 50)
    camera = expression(material, unreal.MaterialExpressionCameraPositionWS, -1600, 150)
    inputs = [(normal, '', 'Normal'), (width, '', 'Width'), (position, '', 'Position'), (camera, '', 'Camera')]
    for i, (key, value) in enumerate([('OutlineReferenceDistance', 400), ('OutlineMinScale', .4),
                                     ('OutlineMaxScale', 1.6), ('OutlineDistanceScale', 1)]):
        inputs.append((scalar_parameter(material, key, value, -1600, 250+i*90), '', key))
    offset = expression(material, unreal.MaterialExpressionCustom, -350, 110)
    offset.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    offset.set_editor_property('inputs', [custom_input(pin) for _, _, pin in inputs])
    offset.set_editor_property('code', '''
float lo = max(OutlineMinScale, 0.0);
float hi = max(OutlineMaxScale, lo);
float scale = clamp(length(Camera - Position) / max(OutlineReferenceDistance, 1.0), lo, hi);
return Normal * max(Width, 0.0) * lerp(1.0, scale, saturate(OutlineDistanceScale));
'''.strip())
    for node, output, pin in inputs:
        assert material_lib.connect_material_expressions(node, output, offset, pin)
    assert material_lib.connect_material_property(offset, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)


def build_outline_master(name="M_PGToonOutline", extended_alpha=False):
    material = recreate_material(name)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_skeletal_mesh", True)
    material.set_editor_property("opacity_mask_clip_value", 0.5)

    color = vector_parameter(material, "OutlineColor", (0.025, 0.02, 0.04, 1.0), -600, -80)
    if not material_lib.connect_material_property(
        color, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ):
        raise RuntimeError("Could not connect outline color")

    normal = expression(material, unreal.MaterialExpressionVertexNormalWS, -620, 80)
    width = scalar_parameter(material, "OutlineWidth", 0.75, -620, 190)
    connect_outline_offset(material, normal, width)

    sign = expression(material, unreal.MaterialExpressionTwoSidedSign, -620, 330)
    backface = expression(material, unreal.MaterialExpressionCustom, -350, 330)
    backface.set_editor_property("code", "return Sign < 0.0 ? 1.0 : 0.0;")
    backface.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    backface.set_editor_property("inputs", [custom_input("Sign")])
    if not material_lib.connect_material_expressions(sign, "", backface, "Sign"):
        raise RuntimeError("Could not connect outline face sign")
    if extended_alpha:
        base_tex = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1000, 450)
        base_tex.set_editor_property('parameter_name', 'BaseTexture')
        base_tex.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
        mask_tex = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1000, 600)
        mask_tex.set_editor_property('parameter_name', 'OpacityTexture')
        mask_tex.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
        settings = [scalar_parameter(material, key, value, -1000, 750+i*90) for i, (key, value) in enumerate([
            ('AlphaMaskMode', 0), ('AlphaMaskScale', 1), ('AlphaMaskValue', 0), ('OpacityCutoff', .333), ('OutlineEnabled', 1),
            ('UseBaseAlpha', 1), ('MainOpacity', 1), ('DissolveAmount', 0)])]
        uv = expression(material, unreal.MaterialExpressionTextureCoordinate, -1000, 1600)
        backface.set_editor_property('code', '''
float mask = saturate(Mask * Scale + Value);
float alpha = lerp(1.0, BaseAlpha, saturate(UseBaseAlpha));
if (Mode >= .5 && Mode < 1.5) alpha = mask;
else if (Mode >= 1.5 && Mode < 2.5) alpha *= mask;
else if (Mode >= 2.5 && Mode < 3.5) alpha = saturate(alpha + mask);
else if (Mode >= 3.5) alpha = saturate(alpha - mask);
float noise = frac(sin(dot(floor(UV * 180.0), float2(12.9898, 78.233))) * 43758.5453);
float alive = DissolveAmount <= 0.0 ? 1.0 : step(saturate(DissolveAmount), noise);
return (Sign < 0.0 ? 1.0 : 0.0) * step(Cutoff, alpha * MainOpacity) * Enabled * alive;
'''.strip())
        backface.set_editor_property('inputs', [custom_input(n) for n in ['Sign', 'BaseAlpha', 'Mask', 'Mode', 'Scale', 'Value', 'Cutoff', 'Enabled', 'UseBaseAlpha', 'MainOpacity', 'DissolveAmount', 'UV']])
        for src, output, pin in [(sign, '', 'Sign'), (base_tex, 'A', 'BaseAlpha'), (mask_tex, 'R', 'Mask'), (uv, '', 'UV')] + [(s, '', p) for s, p in zip(settings, ['Mode', 'Scale', 'Value', 'Cutoff', 'Enabled', 'UseBaseAlpha', 'MainOpacity', 'DissolveAmount'])]:
            assert material_lib.connect_material_expressions(src, output, backface, pin)
    if not material_lib.connect_material_property(
        backface, "", unreal.MaterialProperty.MP_OPACITY_MASK
    ):
        raise RuntimeError("Could not connect outline face mask")

    material_lib.recompile_material(material)
    save(material)
    return material


def import_texture(source: Path):
    require_file(source)
    name = "T_" + re.sub(r"[^A-Za-z0-9_]", "_", source.stem)
    target = f"{destination}/Textures/{name}"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", f"{destination}/Textures")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(target)
    if not texture:
        raise RuntimeError(f"Texture import failed: {source}")
    return texture


def import_mesh():
    source = source_root / manifest["fbx"]
    require_file(source)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", manifest["skeletal_mesh"])
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", unreal.FbxFactory())

    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    skeletal = options.get_editor_property("skeletal_mesh_import_data")
    skeletal.set_editor_property("import_morph_targets", True)
    skeletal.set_editor_property("convert_scene", True)
    skeletal.set_editor_property("convert_scene_unit", True)
    skeletal.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    task.set_editor_property("options", options)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(mesh_path)
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError(f"Skeletal mesh import failed: {mesh_path}")
    skeleton = mesh.get_editor_property("skeleton")
    physics_asset = mesh.get_editor_property("physics_asset")
    if skeleton:
        save(skeleton)
    if physics_asset:
        save(physics_asset)
    save(mesh)
    return mesh


def create_instance(name, parent, texture, opacity_cutoff):
    instance_name = "MI_PGToon_" + re.sub(r"^M_", "", name)
    instance_path = f"{destination}/Materials/{instance_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(instance_path):
        instance = unreal.load_asset(instance_path)
    else:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        instance = tools.create_asset(
            instance_name,
            f"{destination}/Materials",
            unreal.MaterialInstanceConstant,
            factory,
        )
    if not instance:
        raise RuntimeError(f"Could not create {instance_path}")
    instance.set_editor_property("parent", parent)
    material_lib.set_material_instance_texture_parameter_value(
        instance, "BaseTexture", texture
    )
    material_lib.set_material_instance_scalar_parameter_value(
        instance, "OpacityCutoff", opacity_cutoff
    )
    material_lib.update_material_instance(instance)
    save(instance)
    return instance


def main():
    toon_master = build_toon_master()
    outline_master = build_outline_master()
    texture_cache = {}
    instances = {}
    for material_name, relative_texture in manifest["materials"].items():
        source = source_root / relative_texture
        if relative_texture not in texture_cache:
            texture_cache[relative_texture] = import_texture(source)
        cutoff = 0.001 if material_name in manifest["low_cutoff_materials"] else 0.333
        instances[material_name] = create_instance(material_name, toon_master, texture_cache[relative_texture], cutoff)
        apply_shading_profile(instances[material_name], 'Inori', material_name)
        save(instances[material_name])
    mesh = import_mesh()
    skeletal_materials = list(mesh.get_editor_property("materials"))
    assigned, unmatched = [], []
    for index, slot in enumerate(skeletal_materials):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        instance = instances.get(slot_name)
        if instance:
            slot.set_editor_property("material_interface", instance)
            skeletal_materials[index] = slot
            assigned.append(slot_name)
        else:
            unmatched.append(slot_name)
    mesh.set_editor_property("materials", skeletal_materials)
    save(mesh)
    report = {
        "status": "PASS" if not unmatched else "PARTIAL", "source": str(source_root),
        "mesh": mesh.get_path_name(), "skeleton": mesh.get_editor_property("skeleton").get_path_name(),
        "toon_master": toon_master.get_path_name(), "outline_master": outline_master.get_path_name(),
        "texture_count": len(texture_cache), "material_instance_count": len(instances),
        "assigned_slots": assigned, "unmatched_slots": unmatched,
    }
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("PG Toon character test import: " + json.dumps(report, ensure_ascii=False))
    if unmatched:
        raise RuntimeError(f"Unmatched material slots: {unmatched}")


if __name__ == '__main__':
    main()
