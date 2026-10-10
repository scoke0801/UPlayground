"""Import the staged Griffin and carnivorous plant pack into UE 5.8."""
import json, re, traceback
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
ART=ROOT/'Tools/Art/CreatureModels'
DATA=json.loads((ART/'manifest.json').read_text(encoding='utf-8'))
DEST=DATA['destination']
OUT=ROOT/'Saved/CreatureModels'
OUT.mkdir(parents=True,exist_ok=True)
EAL=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
REPORT={'status':'RUNNING','models':[]}
def save(a):
    assert EAL.save_loaded_asset(a,only_if_is_dirty=False),a.get_path_name()
def clean(n): return re.sub('[^A-Za-z0-9_]+','_',n).strip('_')
def task(file,folder,name,options=None):
    t=unreal.AssetImportTask()
    t.filename=str(ART/file);t.destination_path=folder;t.destination_name=name
    t.automated=True;t.save=True;t.replace_existing=True
    if options:
        t.factory=unreal.FbxFactory();t.options=options
    TOOLS.import_asset_tasks([t])
    return [unreal.load_asset(p) for p in t.imported_object_paths]
def import_fbx(file,folder,name,skeleton=None):
    o=unreal.FbxImportUI()
    o.automated_import_should_detect_type=False
    o.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION if skeleton else unreal.FBXImportType.FBXIT_SKELETAL_MESH
    o.import_as_skeletal=True;o.import_mesh=not bool(skeleton)
    o.import_materials=False;o.import_textures=False;o.import_animations=bool(skeleton)
    if skeleton:
        o.skeleton=skeleton
        d=o.anim_sequence_import_data
        d.set_editor_property('animation_length',unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        d.set_editor_property('use_default_sample_rate',False);d.set_editor_property('custom_sample_rate',30)
    else:
        d=o.skeletal_mesh_import_data
        d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    d.convert_scene=True;d.convert_scene_unit=True
    return task(file,folder,name,o)
def main():
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
    for m in DATA['models']:
        folder=DEST+'/'+m['name'];path=folder+'/SK_PG_'+m['name']
        mesh=unreal.load_asset(path) if EAL.does_asset_exist(path) else None
        if not mesh:
            assets=import_fbx(m['mesh'],folder,'SK_PG_'+m['name'])
            mesh=next(a for a in assets if isinstance(a,unreal.SkeletalMesh))
        sk=mesh.get_editor_property('skeleton');save(sk)
        if mesh.get_editor_property('physics_asset'):save(mesh.get_editor_property('physics_asset'))
        save(mesh)
        animfolder=folder+'/Animations'
        clips=[unreal.load_asset(p) for p in EAL.list_assets(animfolder,recursive=True)] if EAL.does_directory_exist(animfolder) else []
        if len(clips)!=len(m['clips']):import_fbx(m['animation'],animfolder,'AS_PG_'+m['name'],sk)
        clips=[unreal.load_asset(p) for p in EAL.list_assets(animfolder,recursive=True)]
        assert len([a for a in clips if isinstance(a,unreal.AnimSequence)])==len(m['clips'])
        row={'name':m['name'],'mesh':mesh.get_path_name(),'skeleton':sk.get_path_name(),'slots':[str(s.material_slot_name) for s in mesh.get_editor_property('materials')],'bounds':str(mesh.get_bounds()),'animations':[]}
        for a in clips:
            if isinstance(a,unreal.AnimSequence):
                save(a);row['animations'].append({'path':a.get_path_name(),'seconds':a.get_play_length()})
        save(sk)
        REPORT['models'].append(row)
        (OUT/'import.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    REPORT['status']='PASS'
try:main()
except Exception:REPORT.update(status='FAIL',error=traceback.format_exc());unreal.log_error(REPORT['error'])
finally:(OUT/'import.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')

if REPORT['status']!='PASS':raise RuntimeError(REPORT['error'])
