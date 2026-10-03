"""Import guardian armor/audio and update only enemy/skill 15103. UE editor commandlet.

Original packages and row snapshots are backed up before writing. Safe to rerun.
"""
import copy
import json
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
SOURCE = ROOT/'Tools/Art/Guardian'
OUT = ROOT/'Saved/Guardian'
DEST = '/Game/Art/Guardian'
DATA = '/Game/DataCenter/Guardian'
OUT.mkdir(parents=True,exist_ok=True)
(OUT/'import.json').write_text('{"status":"RUNNING"}',encoding='utf-8')
BACKUP = ROOT/'Saved/Backups/Guardian'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
manifest = json.loads((SOURCE/'manifest.json').read_text(encoding='utf-8'))
preserved = set()

def preserve(path):
    relative = Path(path.split('.')[0].removeprefix('/Game/')+'.uasset')
    if relative in preserved: return
    preserved.add(relative)
    source = ROOT/'Content'/relative
    if source.is_file():
        target = BACKUP/relative; target.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(source,target)

def save(asset):
    preserve(asset.get_path_name())
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()

def rows(path):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))

enemy_path='/Game/DataCenter/DataTables/Actor/DT_Enemy'
skill_path='/Game/DataCenter/DataTables/Skill/DT_Skill'
enemies,skills = rows(enemy_path),rows(skill_path)
before = dict(enemies=copy.deepcopy(enemies),skills=copy.deepcopy(skills))
if not (OUT/'baseline.json').exists():
    (OUT/'baseline.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')

material_path=DEST+'/'+manifest['material']
preserve(material_path)
mat=unreal.load_asset(material_path) if unreal.EditorAssetLibrary.does_asset_exist(material_path) else tools.create_asset(manifest['material'],DEST,unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(mat)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
color=lib.create_material_expression(mat,unreal.MaterialExpressionVertexColor,-650,0)
assert lib.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
state=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,-650,200)
state.set_editor_property('parameter_name','StateColor'); state.set_editor_property('default_value',unreal.LinearColor(.08,.55,1,1))
glow=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-650,360)
glow.set_editor_property('parameter_name','StateGlow'); glow.set_editor_property('default_value',1.2)
mask=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply,-380,210)
assert lib.connect_material_expressions(state,'RGB',mask,'A')
assert lib.connect_material_expressions(color,'A',mask,'B')
emission=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply,-150,210)
assert lib.connect_material_expressions(mask,'',emission,'A')
assert lib.connect_material_expressions(glow,'',emission,'B')
assert lib.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
for name,value,prop in [('Metallic',.6,unreal.MaterialProperty.MP_METALLIC),('Roughness',.40,unreal.MaterialProperty.MP_ROUGHNESS)]:
    node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant); node.set_editor_property('r',value)
    assert lib.connect_material_property(node,'',prop)
uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-650,530)
dissolve=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-650,680)
dissolve.set_editor_property('parameter_name','DissolveAmount'); dissolve.set_editor_property('default_value',0)
custom=lib.create_material_expression(mat,unreal.MaterialExpressionCustom,-300,580)
custom.set_editor_property('code','return Amount <= 0 ? 1 : (Amount >= 1 ? 0 : step(Amount, frac(sin(dot(floor(UV*75),float2(12.9898,78.233)))*43758.5453)));')
custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
inputs=[]
for name in ['UV','Amount']:
    inp=unreal.CustomInput(); inp.set_editor_property('input_name',name); inputs.append(inp)
custom.set_editor_property('inputs',inputs)
assert lib.connect_material_expressions(uv,'',custom,'UV')
assert lib.connect_material_expressions(dissolve,'',custom,'Amount')
assert lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_OPACITY_MASK)
lib.recompile_material(mat); save(mat)

for entry in manifest['meshes']:
    preserve(DEST+'/'+entry['name'])
    task=unreal.AssetImportTask()
    for key,value in [('filename',str(SOURCE/entry['fbx'])),('destination_path',DEST),('destination_name',entry['name']),
                      ('automated',True),('replace_existing',True),('save',True),('factory',unreal.FbxFactory())]: task.set_editor_property(key,value)
    options=unreal.FbxImportUI()
    for key,value in [('automated_import_should_detect_type',False),('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH),
                      ('import_mesh',True),('import_materials',False),('import_textures',False),('import_animations',False)]: options.set_editor_property(key,value)
    static=options.get_editor_property('static_mesh_import_data')
    for key,value in [('combine_meshes',True),('auto_generate_collision',False),('generate_lightmap_u_vs',False),('convert_scene',True),
                      ('convert_scene_unit',True),('force_front_x_axis',False),('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE),
                      ('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)]: static.set_editor_property(key,value)
    task.set_editor_property('options',options); tools.import_asset_tasks([task])
    mesh=unreal.load_asset(DEST+'/'+entry['name']); assert isinstance(mesh,unreal.StaticMesh)
    # The StaticMeshEditor subsystem is not instantiated by headless commandlets.
    # Explicitly clear reimport-generated simple geometry on this cosmetic mesh.
    body=mesh.get_editor_property('body_setup')
    geometry=body.get_editor_property('agg_geom')
    for field in ['box_elems','sphere_elems','sphyl_elems','convex_elems']:
        geometry.set_editor_property(field,[])
    body.set_editor_property('agg_geom',geometry)
    mesh.set_material(0,mat); save(mesh)

sounds={}
for source in sorted(SOURCE.glob('*.wav')):
    preserve(DEST+'/'+source.stem)
    task=unreal.AssetImportTask()
    for key,value in [('filename',str(source)),('destination_path',DEST),('destination_name',source.stem),('automated',True),('replace_existing',True),('save',True)]: task.set_editor_property(key,value)
    tools.import_asset_tasks([task]); sounds[source.stem]=unreal.load_asset(DEST+'/'+source.stem)
    assert sounds[source.stem]

presentation_path=DATA+'/DA_PGGuardianPresentation'
preserve(presentation_path)
if unreal.EditorAssetLibrary.does_asset_exist(presentation_path): presentation=unreal.load_asset(presentation_path)
else:
    factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',unreal.PGEnemyPresentationData)
    presentation=tools.create_asset('DA_PGGuardianPresentation',DATA,unreal.PGEnemyPresentationData,factory)

# Calibrate socket transforms from the actual authored sword idle, in component space.
idle=unreal.load_asset('/Game/ExternalAssets/Characters/Enemies/SkeletonEnemy/Animations/Anim_Idle_Sword')
pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(idle,0,unreal.AnimPoseEvaluationOptions())
pieces=[]
def piece(name,asset,bone,position,rotation,scale,recovery_position=None,recovery_rotation=None):
    bone_pose=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD)
    target=unreal.Transform(location=position,rotation=rotation,scale=scale)
    relative=unreal.MathLibrary.make_relative_transform(target,bone_pose)
    exposed=relative
    if recovery_position is not None:
        target=unreal.Transform(location=recovery_position,rotation=recovery_rotation,scale=scale)
        exposed=unreal.MathLibrary.make_relative_transform(target,bone_pose)
    entry=unreal.PGEnemyArmorPiece()
    for key,value in [('name',name),('mesh',unreal.load_asset(DEST+'/'+asset)),('socket',bone),('guard_transform',relative),('recovery_transform',exposed)]: entry.set_editor_property(key,value)
    pieces.append(entry)
    return str(bone_pose)
calibration={}
# Skeleton forward is +Y, mesh rotates -90 degrees on the actor.
calibration['shield']=piece('PG_GuardianShield','SM_PG_GuardianShield','hand_l',
    unreal.Vector(30,20,92),unreal.Rotator(pitch=0,yaw=90,roll=0),unreal.Vector(1,1,1),
    unreal.Vector(48,8,66),unreal.Rotator(pitch=-28,yaw=120,roll=-20))
for side,x in [('l',24),('r',-24)]:
    calibration[side]=piece('PG_GuardianShoulder_'+side,'SM_PG_GuardianPauldron','upperarm_'+side,
        unreal.Vector(x,0,136),unreal.Rotator(pitch=0,yaw=90,roll=0),unreal.Vector(.65,.55,.8))
presentation.set_editor_property('armor',pieces)
for field,name in [('windup_sound','S_PG_GuardWindup'),('aim_lock_sound','S_PG_GuardLock'),('recovery_sound','S_PG_GuardRecover'),('guard_hit_sound','S_PG_GuardClang')]: presentation.set_editor_property(field,sounds[name])
presentation.set_editor_property('guard_hit_vfx',unreal.load_asset('/Game/DataCenter/CombatCycle/NS_ImpactCritical'))
presentation.set_editor_property('guard_hit_vfx_scale',.22)
save(presentation)

montage_path=DATA+'/AM_PGGuardianSlam'
preserve(montage_path)
montage=unreal.load_asset(montage_path) if unreal.EditorAssetLibrary.does_asset_exist(montage_path) else unreal.EditorAssetLibrary.duplicate_asset('/Game/Blueprints/Actor/NonPlayer/Enemy/Skeleton/Anim/AM_Skeleton_Sword_Attack_1',montage_path)
assert montage
unreal.AnimationLibrary.remove_all_animation_notify_tracks(montage)
save(montage)
enemy=next(r for r in enemies if r['EnemyID']==15103)
skill=next(r for r in skills if r['SkillID']==15103)
enemy['Presentation']=presentation.get_path_name()
skill.update(ElitePresentationMontage=montage.get_path_name(),bSyncMontageToPattern=True,
             WindupMontageFraction=0.,ImpactMontageFraction=.48,ImpactVFXScale=.42,bHeavyImpactFeedback=False,
             SlamVFX='/Game/DataCenter/CombatCycle/NS_ImpactCritical.NS_ImpactCritical',
             AttackSound=sounds['S_PG_GuardSlam'].get_path_name())
for path,data in [(enemy_path,enemies),(skill_path,skills)]:
    if data==before['enemies' if path==enemy_path else 'skills']:
        continue
    preserve(path); table=unreal.load_asset(path)
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(data,ensure_ascii=False)); save(table)
report=dict(status='PASS',enemy_id=15103,armor_parts=len(pieces),montage=montage.get_path_name(),montage_seconds=montage.get_play_length(),
            calibration=calibration,backup=str(BACKUP),presentation=presentation.get_path_name())
import runpy
runpy.run_path(str(ROOT/'Tools/Validation/ConfigureGuardianMotion.py'),run_name='__main__')
from ValidateGuardianPresentation import validate_guardian_presentation
assert validate_guardian_presentation({r['EnemyID']:r for r in rows(enemy_path)},{r['SkillID']:r for r in rows(skill_path)},before)==1
(OUT/'import.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('PGGuardian import PASS '+json.dumps(report))
