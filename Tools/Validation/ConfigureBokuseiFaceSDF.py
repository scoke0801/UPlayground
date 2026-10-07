"""Create isolated SDF face materials and optionally apply only Bokusei's face MI."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
import ConfigureToonCharacterTest as shared
OUT = ROOT/'Saved/BokuseiFaceSDF'
RUN = Path(os.environ['PG_FACE_SDF_RUN'])
DEST = '/Game/Art/ToonTest/BokuseiFaceSDF'
SOURCE = '/Game/Art/ToonTest/Bokusei/Materials/MI_PGToon_Bokusei_Mat_Bokusei_Face'
BASELINE = DEST+'/Materials/MI_PGBokusei_Face_Baseline'
SDF = DEST+'/Materials/MI_PGBokusei_Face_SDF'
LIT = DEST+'/Materials/MI_PGBokusei_Face_SDFWorld'
LIB = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
REPORT = dict(schema=1,status='RUNNING',run=str(RUN),destination=DEST,source_face=SOURCE,baseline_face=BASELINE,sdf_face=SDF,world_face=LIT)

def package(path):
    return ROOT/'Content'/(path.split('.')[0].removeprefix('/Game/')+'.uasset')

def save(asset):
    assert asset.get_path_name().startswith(DEST+'/') or asset.get_path_name().split('.')[0] == SOURCE
    assert EAL.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()

def copy_parameters(source,target):
    for kind in ['scalar','vector','texture']:
        for name in getattr(LIB,'get_'+kind+'_parameter_names')(source):
            value = getattr(LIB,'get_material_instance_'+kind+'_parameter_value')(source,name)
            if value is not None:
                getattr(LIB,'set_material_instance_'+kind+'_parameter_value')(target,name,value)

def instance(path,parent,source):
    asset = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    for kind in ['scalar','vector','texture']: asset.set_editor_property(kind+'_parameter_values',[])
    LIB.set_material_instance_parent(asset,parent)
    copy_parameters(source,asset)
    return asset

def main():
    source_file = package(SOURCE)
    (RUN/'backup').mkdir(parents=True,exist_ok=True)
    shutil.copy2(source_file,RUN/'backup/original_face.uasset')
    previous = ROOT/'Content/Art/ToonTest/BokuseiFaceSDF'
    if previous.exists(): shutil.copytree(previous,RUN/'backup/sdf_assets')
    bake_path = ROOT/'Tools/Art/ToonTest/BokuseiFaceSDF/bake.json'
    bake = json.loads(bake_path.read_text(encoding='utf-8'))
    assert bake['status'] == 'PASS' and bake['skin_island_conflicts'] == 0
    texture_source = Path(bake['texture'])
    assert hashlib.sha256(texture_source.read_bytes()).hexdigest() == bake['texture_sha256']
    source = unreal.load_asset(SOURCE)
    assert source
    applied_before = source.get_editor_property('parent').get_path_name().startswith(DEST+'/')
    if not EAL.does_asset_exist(BASELINE):
        assert not applied_before,'Missing baseline for an already-applied face material'
        baseline = EAL.duplicate_asset(SOURCE,BASELINE)
        save(baseline)
    baseline = unreal.load_asset(BASELINE)
    assert baseline
    task = unreal.AssetImportTask()
    task.filename,task.destination_path,task.destination_name = str(texture_source),DEST+'/Textures','T_PGBokusei_FaceSDF'
    # Save once after assigning numeric texture settings. Immediate import-save
    # followed by package replacement can race its asynchronous file handles.
    task.automated,task.replace_existing,task.save = True,True,False
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(DEST+'/Textures/T_PGBokusei_FaceSDF')
    assert texture
    texture.set_editor_property('srgb',False)
    # Preserve smooth numeric thresholds; RGB8 + normal mip filtering, no lossy BC1.
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    texture.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
    save(texture)
    shared.MASTER_DEST = DEST+'/Materials'
    master = shared.build_toon_master('M_PGBokuseiFaceSDF',True,False,False,face_sdf_texture=texture)
    world_master = shared.build_toon_master('M_PGBokuseiFaceSDFWorld',True,False,True,face_sdf_texture=texture)
    parameter_source = source if applied_before else baseline
    settings_path = ROOT/'Tools/Art/ToonTest/BokuseiFaceSDF/settings.json'
    parameters = json.loads(settings_path.read_text(encoding='utf-8'))['scalars']
    for path,parent in [(SDF,master),(LIT,world_master)]:
        mi = instance(path,parent,parameter_source)
        for name,value in parameters.items(): LIB.set_material_instance_scalar_parameter_value(mi,name,value)
        LIB.set_material_instance_texture_parameter_value(mi,'FaceSDFTexture',texture)
        if path == LIT:
            for name,value in dict(WorldLightingInfluence=.65,FaceShading=0.,HairAnisotropy=0.,ShadowCast=0.).items():
                LIB.set_material_instance_scalar_parameter_value(mi,name,value)
        LIB.update_material_instance(mi)
        save(mi)
    apply = '-PGApplyBokuseiFaceSDF' in unreal.SystemLibrary.get_command_line()
    if apply:
        LIB.set_material_instance_parent(source,master)
        for name,value in parameters.items(): LIB.set_material_instance_scalar_parameter_value(source,name,value)
        LIB.set_material_instance_texture_parameter_value(source,'FaceSDFTexture',texture)
        LIB.update_material_instance(source)
        save(source)
    protected = bake['head_frame']['protected']
    face_relative = str(source_file.relative_to(ROOT))
    assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest() == h for p,h in protected.items() if p != face_relative)
    REPORT.update(status='PASS',applied=apply or applied_before,texture=texture.get_path_name(),parameters=parameters,
                  bake=str(bake_path),model=bake['mesh'],slot=bake['slot'],protected={p:h for p,h in protected.items() if p != face_relative},
                  baseline_sha256=hashlib.sha256(package(BASELINE).read_bytes()).hexdigest(),
                  applied_sha256=hashlib.sha256(source_file.read_bytes()).hexdigest())

try:
    main()
except Exception:
    REPORT.update(status='FAIL',error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    payload=json.dumps(REPORT,ensure_ascii=False,indent=2)
    (OUT/'configure.json').write_text(payload,encoding='utf-8')
    (RUN/'configure.json').write_text(payload,encoding='utf-8')
if REPORT['status'] != 'PASS': raise RuntimeError('Bokusei face SDF configuration failed')
