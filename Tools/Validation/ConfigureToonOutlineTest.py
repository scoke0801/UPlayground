"""Alpha-aware, per-slot inverted hull for the isolated toon motion gallery."""
import json
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT / 'Tools/Validation'))
from ConfigureToonCharacterTest import connect_outline_offset
DEST = '/Game/Art/ToonTest/Materials'
OUT = ROOT / 'Saved/ToonTest/Outline' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
OUT.mkdir(parents=True)
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
mesh = unreal.load_asset('/Game/Art/ToonTest/Inori/SK_Inori_ToonTest')
name = 'M_PGToonOutlineMasked'
path = DEST + '/' + name


def backup(asset_path):
    relative = asset_path.split('.')[0].removeprefix('/Game/') + '.uasset'
    source = ROOT / 'Content' / relative
    if source.is_file():
        target = OUT / 'backup' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


backup(path)
mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(mat)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)
mat.set_editor_property('used_with_skeletal_mesh', True)


def expr(cls, x, y):
    return lib.create_material_expression(mat, cls, x, y)


def scalar(name, default, x, y):
    node = expr(unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', default)
    return node


color = expr(unreal.MaterialExpressionVectorParameter, -600, -200)
color.set_editor_property('parameter_name', 'OutlineColor')
color.set_editor_property('default_value', unreal.LinearColor(.025, .02, .04, 1))
assert lib.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
normal = expr(unreal.MaterialExpressionVertexNormalWS, -600, 0)
width = scalar('OutlineWidth', .4, -600, 100)
connect_outline_offset(mat, normal, width)
texture = expr(unreal.MaterialExpressionTextureSampleParameter2D, -900, 260)
texture.set_editor_property('parameter_name', 'BaseTexture')
texture.set_editor_property('texture', unreal.load_asset('/Game/Art/ToonTest/Inori/Textures/T_Tex_Inori_BodyBase'))
sign = expr(unreal.MaterialExpressionTwoSidedSign, -900, 430)
cutoff = scalar('OpacityCutoff', .333, -900, 520)
enabled = scalar('OutlineEnabled', 1, -900, 610)
uv = expr(unreal.MaterialExpressionTextureCoordinate, -900, 700)
dissolve = scalar('DissolveAmount', 0, -900, 790)
mask = expr(unreal.MaterialExpressionCustom, -300, 430)
mask.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
mask.set_editor_property('code', '''
float noise = frac(sin(dot(floor(UV * 180.0), float2(12.9898, 78.233))) * 43758.5453);
float alive = DissolveAmount <= 0.0 ? 1.0 : step(saturate(DissolveAmount), noise);
return (Sign < 0.0 ? 1.0 : 0.0) * step(OpacityCutoff, Alpha) * OutlineEnabled * alive;
'''.strip())
connections = [(sign, '', 'Sign'), (texture, 'A', 'Alpha'), (cutoff, '', 'OpacityCutoff'),
               (enabled, '', 'OutlineEnabled'), (uv, '', 'UV'), (dissolve, '', 'DissolveAmount')]
inputs = []
for _, _, input_name in connections:
    value = unreal.CustomInput()
    value.set_editor_property('input_name', input_name)
    inputs.append(value)
mask.set_editor_property('inputs', inputs)
for node, output, input_name in connections:
    assert lib.connect_material_expressions(node, output, mask, input_name)
assert lib.connect_material_property(mask, '', unreal.MaterialProperty.MP_OPACITY_MASK)
lib.recompile_material(mat)
save(mat)
instances = []
for slot in mesh.get_editor_property('materials'):
    slot_name = str(slot.material_slot_name)
    mi_name = 'MI_PGToonOutline_' + slot_name.removeprefix('M_')
    mi_path = '/Game/Art/ToonTest/Inori/Materials/' + mi_name
    backup(mi_path)
    mi = unreal.load_asset(mi_path) if unreal.EditorAssetLibrary.does_asset_exist(mi_path) else tools.create_asset(mi_name, '/Game/Art/ToonTest/Inori/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mi, mat)
    base = slot.material_interface
    tex = lib.get_material_instance_texture_parameter_value(base, 'BaseTexture')
    alpha_cutoff = lib.get_material_instance_scalar_parameter_value(base, 'OpacityCutoff')
    assert tex
    lib.set_material_instance_texture_parameter_value(mi, 'BaseTexture', tex)
    lib.set_material_instance_scalar_parameter_value(mi, 'OpacityCutoff', alpha_cutoff)
    # No inflated hull on facial alpha overlays. Other slots retain texture alpha.
    active = slot_name not in ['M_Inori_EyeAlpha', 'M_Inori_Expressions']
    slot_width = .08 if slot_name == 'M_Inori_Glasses' else (.12 if slot_name == 'M_Inori_Head' else .4)
    lib.set_material_instance_scalar_parameter_value(mi, 'OutlineWidth', slot_width)
    lib.set_material_instance_scalar_parameter_value(mi, 'OutlineEnabled', 1 if active else 0)
    save(mi)
    instances.append({'slot': slot_name, 'asset': mi_path, 'width_cm': slot_width, 'enabled': active})
# The source EyeAlpha/Glasses materials are alpha blended, not merely cutouts.
# Separate overlay instances preserve partial alpha without editing mesh defaults.
overlay_path = DEST + '/M_PGToonAlphaOverlay'
backup(overlay_path)
overlay = unreal.load_asset(overlay_path) if unreal.EditorAssetLibrary.does_asset_exist(overlay_path) else tools.create_asset('M_PGToonAlphaOverlay', DEST, unreal.Material, unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(overlay)
overlay.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
overlay.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
overlay.set_editor_property('two_sided', False)
overlay.set_editor_property('used_with_skeletal_mesh', True)
overlay_tex = lib.create_material_expression(overlay, unreal.MaterialExpressionTextureSampleParameter2D, -300, 0)
overlay_tex.set_editor_property('parameter_name', 'BaseTexture')
overlay_tex.set_editor_property('texture', unreal.load_asset('/Game/Art/ToonTest/Inori/Textures/T_Tex_Inori_Head'))
assert lib.connect_material_property(overlay_tex, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(overlay_tex, 'A', unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(overlay)
save(overlay)
overlays = {}
for slot in mesh.get_editor_property('materials'):
    slot_name = str(slot.material_slot_name)
    if slot_name not in ['M_Inori_EyeAlpha', 'M_Inori_Glasses']:
        continue
    mi_name = 'MI_PGToonAlpha_' + slot_name.removeprefix('M_')
    mi_path = '/Game/Art/ToonTest/Inori/Materials/' + mi_name
    backup(mi_path)
    mi = unreal.load_asset(mi_path) if unreal.EditorAssetLibrary.does_asset_exist(mi_path) else tools.create_asset(mi_name, '/Game/Art/ToonTest/Inori/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mi, overlay)
    lib.set_material_instance_texture_parameter_value(mi, 'BaseTexture', lib.get_material_instance_texture_parameter_value(slot.material_interface, 'BaseTexture'))
    save(mi)
    overlays[slot_name] = mi_path
payload = json.dumps({'status': 'PASS', 'master': path, 'instances': instances, 'overlays': overlays, 'run': str(OUT)}, indent=2)
(OUT / 'outline.json').write_text(payload, encoding='utf-8')
(ROOT / 'Saved/ToonTest/outline.json').write_text(payload, encoding='utf-8')
unreal.log('PG Toon alpha outline PASS')
