"""Connect imported models to the playable catalog, preserving Hichi's save ID."""
import copy
import json
import os
import sys
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from PlayableCharacterCatalog import PLAYER_IDS, MODEL_REPLACEMENTS, portrait_source
from PlayableCharacterPolish import (preflight,snapshot,apply_rig,apply_retarget,apply_appearance,
    validate_appearance,STAMP,digest)
from PlayableCharacterTransaction import Transaction,write_json

RUN=Path(os.environ['PG_CHARACTER_RUN'])
SOURCE=ROOT/'Tools/Validation/Data/PlayableCharacterPolish.json'
DEST='/Game/DataCenter/Characters'
UI='/Game/DataCenter/UI/Moonlit'
EAL=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
TX=Transaction(ROOT,RUN)
REPORT=dict(status='RUNNING',characters=[])


def own(name,cls,factory):
    path=DEST+'/'+name
    assert path in TX.data['packages']
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(name,DEST,cls,factory)


def save(asset):
    TX.mark_written(asset.get_path_name().split('.')[0])
    if isinstance(asset,(unreal.PGCharacterAppearance,unreal.IKRigDefinition,unreal.IKRetargeter)):
        EAL.set_metadata_tag(asset,STAMP,digest(snapshot(asset)))
    assert EAL.save_loaded_asset(asset,only_if_is_dirty=False)


def main():
    source=json.loads(SOURCE.read_text(encoding='utf-8'))
    preflight(source)
    baseline={p:snapshot(unreal.load_asset(p)) for p in source['assets']}
    result=dict(schema_version=1,engine_association='5.8',assets=copy.deepcopy(baseline))
    paths=[DEST+'/'+prefix+identity+suffix for identity in MODEL_REPLACEMENTS
        for prefix,suffix in [('DA_',''),('IK_','_Source'),('IK_','_Target'),('RTG_','')]]
    paths+=['/Game/DataCenter/Progression/DA_PGProgression']+[UI+'/T_'+n for n in MODEL_REPLACEMENTS]
    for identity in MODEL_REPLACEMENTS:
        assert portrait_source(ROOT,identity).is_file()
    TX.prepare(paths,SOURCE)
    for identity,(donor,name) in MODEL_REPLACEMENTS.items():
        mesh=unreal.load_asset('/Game/Art/PlayerModels/'+name+'/SK_PG_'+name)
        assert mesh
        for side in ('Source','Target'):
            definition=copy.deepcopy(baseline[DEST+'/IK_'+donor+'_'+side])
            if side=='Target':definition['mesh']=mesh.get_path_name()
            rig=own('IK_'+identity+'_'+side,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
            apply_rig(rig,definition);save(rig)
            result['assets'][DEST+'/IK_'+identity+'_'+side]=snapshot(rig)
        definition=json.loads(json.dumps(baseline[DEST+'/RTG_'+donor]).replace('IK_'+donor+'_','IK_'+identity+'_'))
        definition['target']['mesh']=mesh.get_path_name()
        # New Blender-composed rigs have new local axes; derive a fresh alignment.
        for pose in definition['target']['poses'].values():pose['rotations']={}
        retarget=own('RTG_'+identity,unreal.IKRetargeter,unreal.IKRetargetFactory())
        apply_retarget(retarget,definition)
        ctl=unreal.IKRetargeterController.get_controller(retarget)
        ctl.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
        save(retarget);result['assets'][DEST+'/RTG_'+identity]=snapshot(retarget)
        definition=copy.deepcopy(baseline[DEST+'/DA_'+donor]);fields=definition['fields']
        fields.update(id=identity,display_name=name,mesh=mesh.get_path_name(),parts=[],grip_profiles=[],
            retargeter=retarget.get_path_name(),reconstruct_scaled_translations=True,
            mesh_transform=unreal.Transform().export_text())
        target=unreal.IKRigController.get_controller(unreal.load_asset(DEST+'/IK_'+identity+'_Target'))
        head=target.get_ref_pose_transform_of_bone(fields['head_bone'])
        fields['head_forward_axis']=unreal.MathLibrary.inverse_transform_direction(head,unreal.Vector(0,1,0)).export_text()
        fields['head_right_axis']=unreal.MathLibrary.inverse_transform_direction(head,unreal.Vector(1,0,0)).export_text()
        factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.PGCharacterAppearance)
        appearance=own('DA_'+identity,unreal.PGCharacterAppearance,factory)
        apply_appearance(appearance,definition);validate_appearance(appearance)
        task=unreal.AssetImportTask()
        task.filename=str(portrait_source(ROOT,identity))
        task.destination_path=UI;task.destination_name='T_'+identity
        task.automated=True;task.replace_existing=True;task.save=False
        TOOLS.import_asset_tasks([task])
        portrait=unreal.load_asset(UI+'/T_'+identity);assert portrait
        for key,value in dict(lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,
            compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,
            mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            max_texture_size=1024,srgb=True,compression_no_alpha=False).items():portrait.set_editor_property(key,value)
        save(portrait)
        appearance.set_editor_property('portrait',portrait);save(appearance)
        result['assets'][DEST+'/DA_'+identity]=snapshot(appearance)
        REPORT['characters'].append(dict(id=identity,display_name=name,mesh=mesh.get_path_name(),portrait=portrait.get_path_name()))
    catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
    catalog.set_editor_property('playable_characters',[unreal.load_asset(DEST+'/DA_'+n) for n in PLAYER_IDS]);save(catalog)
    for p,value in baseline.items():
        if p not in paths:assert snapshot(unreal.load_asset(p))==value,p
    write_json(RUN/'connected-source.json',result)
    TX.data['status']='APPLIED_UNVERIFIED';TX.flush()
    REPORT['status']='APPLIED'


try:main()
except Exception:
    REPORT.update(status='FAIL',error=traceback.format_exc());unreal.log_error(REPORT['error'])
finally:write_json(RUN/'connect.json',REPORT)
if REPORT['status']!='APPLIED':raise RuntimeError(REPORT.get('error','Connection failed'))
