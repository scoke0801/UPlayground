"""Build the cooked UI liquid material from the adjacent HLSL source in Unreal."""
from pathlib import Path
import unreal

path='/Game/UI/Combat/M_PGResourceLiquid'
material=unreal.load_asset(path)
if material is None:
    material=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_PGResourceLiquid','/Game/UI/Combat',unreal.Material,unreal.MaterialFactoryNew())
assert isinstance(material,unreal.Material)
lib=unreal.MaterialEditingLibrary
lib.delete_all_material_expressions(material)
material.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
custom=lib.create_material_expression(material,unreal.MaterialExpressionCustom,0,0)
custom.set_editor_property('code',Path(__file__).with_name('ResourceLiquid.ush').read_text())
custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
parameters={'Fill':0.65,'LiquidTime':0.0,'Agitation':0.0,'Phase':0.0,
            'WaveAmplitude':0.035,'WaveSpeed':1.0,'FlowStrength':0.55}
names=['UV','LiquidColor',*parameters]
inputs=[]
for name in names:
    entry=unreal.CustomInput()
    entry.set_editor_property('input_name',name)
    inputs.append(entry)
custom.set_editor_property('inputs',inputs)
uv=lib.create_material_expression(material,unreal.MaterialExpressionTextureCoordinate,-500,-200)
lib.connect_material_expressions(uv,'',custom,'UV')
color=lib.create_material_expression(material,unreal.MaterialExpressionVectorParameter,-500,0)
color.set_editor_property('parameter_name','LiquidColor')
color.set_editor_property('default_value',unreal.LinearColor(0.58,0.012,0.023,1))
lib.connect_material_expressions(color,'RGB',custom,'LiquidColor')
for i,(name,value) in enumerate(parameters.items()):
    node=lib.create_material_expression(material,unreal.MaterialExpressionScalarParameter,-500,150+i*90)
    node.set_editor_property('parameter_name',name)
    node.set_editor_property('default_value',value)
    lib.connect_material_expressions(node,'',custom,name)
rgb=lib.create_material_expression(material,unreal.MaterialExpressionComponentMask,300,0)
alpha=lib.create_material_expression(material,unreal.MaterialExpressionComponentMask,300,150)
for node,channels in [(rgb,'rgb'),(alpha,'a')]:
    for channel in 'rgba':node.set_editor_property(channel,channel in channels)
    lib.connect_material_expressions(custom,'',node,'Input')
lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
unreal.log('PGResourceLiquid CREATE PASS')
