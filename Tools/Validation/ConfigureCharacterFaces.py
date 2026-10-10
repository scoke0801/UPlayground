"""Create model-specific SDF candidates without changing live character materials."""
import json
from pathlib import Path
import sys
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
import ConfigureToonCharacterTest as shared
LIB=unreal.MaterialEditingLibrary
EAL=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
DEST='/Game/Art/ToonTest/Improvement/Faces'
source=json.loads((ROOT/'Tools/Art/ToonTest/CharacterFaces/bakes.json').read_text(encoding='utf-8'))
report=dict(status='RUNNING',characters=[])
shared.MASTER_DEST=DEST
masters={}
try:
    assert source['status']=='PASS', 'All face geometry/UV checks must pass before generating candidates'
    for row in source['characters']:
        identity=row['id'];folder=Path(row['output'])
        bake=json.loads((folder/'bake.json').read_text())
        assert bake['status']=='PASS' and bake['skin_island_conflicts']==0
        task=unreal.AssetImportTask()
        task.filename=str(folder/'T_PGBokusei_FaceSDF.png')
        task.destination_path=DEST+'/Textures';task.destination_name='T_PGFaceSDF_'+identity
        task.automated=True;task.replace_existing=True;task.save=False
        TOOLS.import_asset_tasks([task])
        texture=unreal.load_asset(task.destination_path+'/'+task.destination_name)
        texture.set_editor_property('srgb',False)
        texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
        texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        texture.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP)
        texture.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
        shared.save(texture)
        original=unreal.load_asset(row['slot']['material'])
        parent=original.get_editor_property('parent')
        lit=parent.get_editor_property('shading_model')==unreal.MaterialShadingModel.MSM_DEFAULT_LIT
        if lit not in masters:
            masters[lit]=shared.build_toon_master('M_PGCharacterFaceSDF'+('World' if lit else ''),True,False,lit,texture)
        path=DEST+'/MI_PGFaceSDF_'+identity
        mi=unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset('MI_PGFaceSDF_'+identity,DEST,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        LIB.set_material_instance_parent(mi,masters[lit])
        for kind in ['scalar','vector','texture']:
            for name in getattr(LIB,'get_'+kind+'_parameter_names')(original):
                value=getattr(LIB,'get_material_instance_'+kind+'_parameter_value')(original,name)
                if value is not None:getattr(LIB,'set_material_instance_'+kind+'_parameter_value')(mi,name,value)
        settings=json.loads((folder/'settings.json').read_text())['scalars']
        for name,value in settings.items():LIB.set_material_instance_scalar_parameter_value(mi,name,value)
        LIB.set_material_instance_texture_parameter_value(mi,'FaceSDFTexture',texture)
        LIB.update_material_instance(mi);shared.save(mi)
        report['characters'].append(dict(id=identity,appearance=row['appearance'],index=row['slot']['index'],
            source=original.get_path_name(),candidate=mi.get_path_name(),texture=texture.get_path_name(),world_lit=lit))
    report['status']='PASS'
except BaseException:
    report.update(status='FAIL',error=traceback.format_exc());unreal.log_error(report['error'])
finally:
    (ROOT/'Saved/ToonImprovement/character-faces.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
if report['status']!='PASS':raise RuntimeError(report['error'])
