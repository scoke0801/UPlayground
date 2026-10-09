"""Import standalone source meshes and inspect their real UE dimensions.

Run with UE's Python commandlet. No Unity scenes or assembled arena are used.
"""
import hashlib
import json
from pathlib import Path
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = Path(r'C:\UsingProject\UnityProject\UPlayground\Assets\ExternalAssets\Environment\LowPolyFantasyArena2')
KIT = ROOT/'Tools/Art/ForestRuins'
OUT = ROOT/'Saved/ForestRuins'
DEST = '/Game/Environment/ForestRuins'
OUT.mkdir(parents=True, exist_ok=True)
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
REPORT = dict(status='RUNNING', source=str(SOURCE), meshes={}, textures={}, protected={}, gameplay={})

def fingerprint(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def vec(v):
    return [round(getattr(v, axis), 4) for axis in ('x','y','z')]

try:
    for path in [ROOT/'Content/Maps/RogueArena.umap', ROOT/'Config/DefaultEngine.ini',
                 ROOT/'Content/DataCenter/DataTables/Stage/DT_StageData.uasset',
                 ROOT/'Content/DataCenter/Progression/DA_PGProgression.uasset']:
        REPORT['protected'][str(path.relative_to(ROOT))] = fingerprint(path)
    groups = {
        'Nature': ['Tree_01','Tree_02','Tree_03','Tree_04','Rock_01','Rock_02','Rock_03','Rock_04',
                   'BigRock_01','Log_01','Log_02','Roots_01','Grass_01','Grass_02','Grass_03','Plant_01'],
        'Ruins': ['Ruins_'+str(i).zfill(2) for i in range(1,17)]+['Tile_01','Tile_02','Tile_03','Tile_04'],
        'Props': ['Lantern_01','Barrel_01','Fence_01','Wood_01'],
    }
    tasks=[]
    manifest=dict(schema=1,source_root=str(SOURCE),destination=DEST,meshes=[],textures=[],
                  excludes=['Unity scenes','Unity prefab placements','Arena_01.fbx'],
                  license_note='Imported from the user-owned Unity project; check purchase-specific terms before distribution.')
    for group, names in groups.items():
        for name in names:
            source=SOURCE/'Models'/group/(name+'.fbx')
            assert source.is_file(), source
            target=KIT/'Source/Models'/group/source.name
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(source,target)
            entry=dict(name='SM_PGFR_'+name,source=str(source),file=str(target.relative_to(KIT)),sha256=fingerprint(source))
            manifest['meshes'].append(entry)
            task=unreal.AssetImportTask()
            for key,value in dict(filename=str(target),destination_path=DEST+'/Meshes',destination_name=entry['name'],
                                  automated=True,replace_existing=False,save=True,factory=unreal.FbxFactory()).items():
                task.set_editor_property(key,value)
            options=unreal.FbxImportUI()
            for key,value in dict(automated_import_should_detect_type=False,mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH,
                                  import_mesh=True,import_materials=False,import_textures=False,import_animations=False).items():
                options.set_editor_property(key,value)
            data=options.static_mesh_import_data
            for key,value in dict(combine_meshes=True,auto_generate_collision=True,generate_lightmap_u_vs=False,
                                  convert_scene=True,convert_scene_unit=True,force_front_x_axis=False,
                                  normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():
                data.set_editor_property(key,value)
            task.set_editor_property('options',options)
            if not EAL.does_asset_exist(DEST+'/Meshes/'+entry['name']): tasks.append(task)
    for name in ['T_LowPolyFantasyArena2_Main','T_LowPolyFantasyArena2_Grass','T_LowPolyFantasyArena2_Grass_Normal',
                 'T_LowPolyFantasyArena2_Mud']:
        source=SOURCE/'Textures'/(name+'.png')
        target=KIT/'Source/Textures'/source.name
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
        entry=dict(name=name.replace('T_LowPolyFantasyArena2_', 'T_PGFR_'),source=str(source),file=str(target.relative_to(KIT)),sha256=fingerprint(source))
        manifest['textures'].append(entry)
        task=unreal.AssetImportTask()
        for key,value in dict(filename=str(target),destination_path=DEST+'/Textures',destination_name=entry['name'],
                              automated=True,replace_existing=False,save=True).items(): task.set_editor_property(key,value)
        if not EAL.does_asset_exist(DEST+'/Textures/'+entry['name']): tasks.append(task)
    (KIT/'sources.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    if tasks: TOOLS.import_asset_tasks(tasks)
    for entry in manifest['meshes']:
        mesh=unreal.load_asset(DEST+'/Meshes/'+entry['name'])
        assert isinstance(mesh,unreal.StaticMesh), entry['name']
        box=mesh.get_bounding_box()
        REPORT['meshes'][entry['name']]=dict(path=mesh.get_path_name(),min=vec(box.min),max=vec(box.max),
            triangles=mesh.get_num_triangles(0),slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')])
    for entry in manifest['textures']:
        texture=unreal.load_asset(DEST+'/Textures/'+entry['name'])
        assert isinstance(texture,unreal.Texture2D), entry['name']
        REPORT['textures'][entry['name']]=texture.get_path_name()
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.load_level('/Game/Maps/RogueArena')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    REPORT['gameplay']['game_mode']=world.get_world_settings().get_editor_property('default_game_mode').get_path_name()
    REPORT['gameplay']['actors']=[dict(label=a.get_actor_label(),cls=a.get_class().get_path_name(),
        location=vec(a.get_actor_location()),bounds=[vec(v) for v in a.get_actor_bounds(False)]) for a in actors
        if isinstance(a,(unreal.PlayerStart,unreal.NavMeshBoundsVolume,unreal.PostProcessVolume))]
    stage=unreal.load_asset('/Game/DataCenter/DataTables/Stage/DT_StageData')
    REPORT['gameplay']['stages']=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(stage))
    for relative,expected in REPORT['protected'].items(): assert fingerprint(ROOT/relative)==expected, relative
    REPORT['status']='PASS'
    unreal.log('PGForestResources IMPORT PASS')
except BaseException as error:
    REPORT.update(status='FAIL',error=str(error),traceback=traceback.format_exc())
    unreal.log_error(REPORT['traceback'])
    raise
finally:
    (OUT/'import.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
