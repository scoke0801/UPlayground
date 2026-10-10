"""Create project-owned attack materials and connect profiles; preserve gameplay and backups."""
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from PlayerNiagaraData import profiles, snapshot

DEST = '/Game/Art/PlayerCombatFX/Authored'
STYLES = json.loads((HERE/'styles.json').read_text())
assets = profiles()
validate = unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())[1]
validate = 'PGCombatVFXValidate' in validate
fields = ('slash_material','slash_tint','slash_duration','slash_width','slash_intensity','slash_height','reverse_slash')

def gameplay(asset):
    return {k:v for k,v in snapshot(asset).items() if k not in fields}

def check():
    for skill, asset in assets.items():
        style = STYLES[str(skill)]
        assert asset.get_editor_property('build_reactive_vfx'), skill
        assert [s.name for s in asset.get_editor_property('swing_shapes')] == style['shapes'], skill
        assert asset.get_editor_property('slash_material').get_path_name().startswith(DEST), skill
        assert asset.get_editor_property('use_authored_vfx'), skill

if validate:
    check()
    backup = Path((ROOT/'Saved/CombatVFX_LastBackup.txt').read_text())
    before = json.loads((backup/'gameplay.json').read_text())
    assert {str(k):gameplay(v) for k,v in assets.items()} == before
    unreal.log('PGCombatVFX VALIDATION PASS profiles=8 gameplay_unchanged=1')
else:
    backup = ROOT/'Saved/Backups/CombatVFX'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup.mkdir(parents=True)
    before = {str(k):gameplay(v) for k,v in assets.items()}
    (backup/'gameplay.json').write_text(json.dumps(before,indent=2))
    for asset in list(assets.values()) + [unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(DEST)]:
        if not asset: continue
        relative = asset.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset'
        path = ROOT/'Content'/relative
        target = backup/relative
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,target)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = unreal.load_asset(DEST+'/M_PGCombatVFX')
    if not material:
        material = tools.create_asset('M_PGCombatVFX',DEST,unreal.Material,unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided',True)
    custom = lib.create_material_expression(material,unreal.MaterialExpressionCustom)
    custom.set_editor_property('code',(HERE/'CombatVFX.ush').read_text())
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    scalars = {'Shape':0.,'Progress':.15,'BladeWidth':.15,'Intensity':2.4,'Direction':1.,'CosAngle':.5}
    vectors = {'Tint':(.12,.7,1,1),'BuildWeights':(0,0,0,0),'BleedTint':(1,.035,.12,1),'ShockTint':(.08,.55,1,1),'FrenzyTint':(1,.42,.035,1)}
    inputs = []
    for name in ['UV',*scalars,*vectors]:
        entry = unreal.CustomInput(); entry.set_editor_property('input_name',name); inputs.append(entry)
    custom.set_editor_property('inputs',inputs)
    uv = lib.create_material_expression(material,unreal.MaterialExpressionTextureCoordinate)
    assert lib.connect_material_expressions(uv,'',custom,'UV')
    for name,value in scalars.items():
        node = lib.create_material_expression(material,unreal.MaterialExpressionScalarParameter)
        node.set_editor_property('parameter_name','HalfAngleCos' if name=='CosAngle' else name)
        node.set_editor_property('default_value',value)
        assert lib.connect_material_expressions(node,'',custom,name)
    for name,value in vectors.items():
        node = lib.create_material_expression(material,unreal.MaterialExpressionVectorParameter)
        node.set_editor_property('parameter_name',name)
        node.set_editor_property('default_value',unreal.LinearColor(*value))
        assert lib.connect_material_expressions(node,'',custom,name)
    for channels,prop in [('rgb',unreal.MaterialProperty.MP_EMISSIVE_COLOR),('a',unreal.MaterialProperty.MP_OPACITY)]:
        mask = lib.create_material_expression(material,unreal.MaterialExpressionComponentMask)
        for channel in 'rgba': mask.set_editor_property(channel,channel in channels)
        assert lib.connect_material_expressions(custom,'',mask,'')
        assert lib.connect_material_property(mask,'',prop)
    lib.layout_material_expressions(material); lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
    for skill,asset in assets.items():
        style = STYLES[str(skill)]
        name = 'MI_PGSkill_'+str(skill)
        instance = unreal.load_asset(DEST+'/'+name)
        if not instance: instance = tools.create_asset(name,DEST,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(instance,material)
        lib.set_material_instance_vector_parameter_value(instance,'Tint',unreal.LinearColor(*style['tint'],1))
        lib.set_material_instance_scalar_parameter_value(instance,'Shape',float(getattr(unreal.PGPlayerVFXShape,style['shapes'][0]).value))
        assert unreal.EditorAssetLibrary.save_loaded_asset(instance,only_if_is_dirty=False)
        asset.set_editor_property('slash_material',instance)
        asset.set_editor_property('use_authored_vfx',True)
        asset.set_editor_property('build_reactive_vfx',True)
        asset.set_editor_property('swing_shapes',[getattr(unreal.PGPlayerVFXShape,s) for s in style['shapes']])
        asset.set_editor_property('default_swing_shape',getattr(unreal.PGPlayerVFXShape,style['shapes'][-1]))
        asset.set_editor_property('slash_tint',unreal.LinearColor(*style['tint'],1))
        asset.set_editor_property('slash_width',style['width'])
        asset.set_editor_property('slash_duration',style['duration'])
        asset.set_editor_property('slash_intensity',2.8)
        assert gameplay(asset)==before[str(skill)],skill
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
    check()
    (ROOT/'Saved/CombatVFX_LastBackup.txt').write_text(str(backup))
    unreal.log('PGCombatVFX APPLY PASS profiles=8 materials=9 gameplay_unchanged=1 backup='+str(backup))
