"""Create an isolated native Toon BSDF candidate; never change project settings."""
import json
from pathlib import Path
import sys
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
import ConfigureToonCharacterTest as shared
OUT=ROOT/'Saved/ToonImprovement/Native'
OUT.mkdir(parents=True,exist_ok=True)
DEST='/Game/Art/ToonTest/Improvement/Native'
LIB=unreal.MaterialEditingLibrary
EAL=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
report=dict(status='RUNNING',materials=[])

def own(name,cls,factory):
    path=DEST+'/'+name
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(name,DEST,cls,factory)

try:
    assert unreal.SystemLibrary.get_console_variable_int_value('r.Substrate')==1
    assert unreal.SystemLibrary.get_console_variable_int_value('r.Substrate.ProjectGBufferFormat')==0
    profile=own('TP_PGCharacter',unreal.ToonProfile,unreal.ToonProfileFactory())
    settings=profile.get_editor_property('settings')
    settings.set_editor_property('diffuse_indirect_scale',.35)
    settings.set_editor_property('specular_indirect_scale',.15)
    settings.set_editor_property('diffuse_ramp_include_shadow',True)
    profile.set_editor_property('settings',settings)
    shared.save(profile)
    shared.MASTER_DEST=DEST
    masters={}
    for transparent in [False,True]:
        master=shared.build_toon_master('M_PGNativeToon'+('Transparent' if transparent else ''),True,transparent,True)
        nodes=LIB.get_material_expressions(master)
        texture=next(n for n in nodes if isinstance(n,unreal.MaterialExpressionTextureSampleParameter2D) and str(n.get_editor_property('parameter_name'))=='BaseTexture')
        tint=next(n for n in nodes if isinstance(n,unreal.MaterialExpressionVectorParameter) and str(n.get_editor_property('parameter_name'))=='BaseTint')
        color=shared.expression(master,unreal.MaterialExpressionMultiply,-200,-600)
        assert LIB.connect_material_expressions(texture,'RGB',color,'A')
        assert LIB.connect_material_expressions(tint,'RGB',color,'B')
        toon=shared.expression(master,unreal.MaterialExpressionSubstrateToonBSDF,300,-600)
        toon.set_editor_property('toon_profile',profile)
        assert LIB.connect_material_expressions(color,'',toon,'BaseColor')
        for name,value in [('Roughness',.75),('Specular',.15),('Metallic',0)]:
            assert LIB.connect_material_expressions(shared.scalar_parameter(master,'Native'+name,value,-200,-800),'',toon,name)
        assert LIB.connect_material_property(toon,'',unreal.MaterialProperty.MP_FRONT_MATERIAL)
        LIB.recompile_material(master)
        shared.save(master)
        masters[transparent]=master
    mesh=unreal.load_asset('/Game/Art/ToonTest/Bokusei/SK_Bokusei_ToonTest')
    for i,slot in enumerate(mesh.materials):
        source=slot.material_interface
        parent=source.get_editor_property('parent')
        transparent=parent.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_TRANSLUCENT
        mi=own('MI_PGNative_'+str(slot.material_slot_name),unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        LIB.set_material_instance_parent(mi,masters[transparent])
        for kind in ['scalar','vector','texture']:
            for name in getattr(LIB,'get_'+kind+'_parameter_names')(source):
                value=getattr(LIB,'get_material_instance_'+kind+'_parameter_value')(source,name)
                if value is not None:getattr(LIB,'set_material_instance_'+kind+'_parameter_value')(mi,name,value)
        LIB.update_material_instance(mi)
        shared.save(mi)
        report['materials'].append(dict(index=i,slot=str(slot.material_slot_name),source=source.get_path_name(),candidate=mi.get_path_name()))
    report.update(status='PASS',profile=profile.get_path_name(),substrate=1,gbuffer=0)
except BaseException:
    report.update(status='FAIL',error=traceback.format_exc())
    unreal.log_error(report['error'])
finally:
    (OUT/'configure.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
if report['status']!='PASS':raise RuntimeError(report['error'])
