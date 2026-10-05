"""Connect existing skeleton bow motion/arrow art to both single-shot patterns.

UE Python commandlet; -PGArcherValidate reloads without editing any assets.
"""
import copy
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/DataCenter/SkeletonArcher'
MONTAGE = DEST+'/AM_PGSkeletonArcherShot'
PROJECTILE = DEST+'/BP_PGSkeletonArrow'
ARROW = '/Game/ExternalAssets/Characters/Enemies/SkeletonEnemy/Mesh/Weapon/Bow/Arrow/SM_Arrow'
SOURCE = '/Game/Blueprints/Actor/NonPlayer/Enemy/Skeleton/Anim/AM_Skeleton_Bow_Attack'
SEQUENCE = '/Game/ExternalAssets/Characters/Enemies/SkeletonEnemy/Animations/Anim_Attack'
TABLE = '/Game/DataCenter/DataTables/Skill/DT_Skill'
EAL = unreal.EditorAssetLibrary
IDS = {15102, 15112}


def skill_patch():
    source = unreal.load_asset(SOURCE)
    release = [unreal.AnimationLibrary.get_anim_notify_event_trigger_time(e)
               for e in unreal.AnimationLibrary.get_animation_notify_events(source)
               if str(e.get_editor_property('notify_name'))=='AN_SpawnProjectile_C']
    assert len(release)==1 and 0 < release[0] < source.get_play_length()
    return dict(ProjectileCount=1, ProjectileSpreadHalfAngle=0.,
        ProjectileClass=PROJECTILE+'.BP_PGSkeletonArrow_C',
        MontagePath=dict(AssetPath=dict(PackageName=MONTAGE, AssetName=MONTAGE.rsplit('/',1)[1]), SubPathString=''),
        ElitePresentationMontage=MONTAGE+'.AM_PGSkeletonArcherShot',
        bSyncMontageToPattern=True, WindupMontageFraction=0., ImpactMontageFraction=release[0]/source.get_play_length(),
        bHeavyImpactFeedback=False)


def rows():
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(TABLE)))


def save(asset):
    assert EAL.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def validate():
    montage = unreal.load_asset(MONTAGE)
    sequence = unreal.load_asset(SEQUENCE)
    assert montage and sequence
    assert montage.get_editor_property('skeleton') == sequence.get_editor_property('skeleton')
    assert not unreal.AnimationLibrary.get_animation_notify_events(montage)
    segments = [s for t in montage.get_editor_property('slot_anim_tracks')
                for s in t.get_editor_property('anim_track').get_editor_property('anim_segments')]
    assert len(segments)==1 and segments[0].get_editor_property('anim_reference')==sequence
    assert not unreal.AnimationLibrary.get_animation_notify_events(sequence)
    cls = EAL.load_blueprint_class(PROJECTILE)
    cdo = unreal.get_default_object(cls)
    assert isinstance(cdo, unreal.PGPatternProjectile)
    mesh = cdo.get_editor_property('mesh_component')
    assert mesh.get_editor_property('static_mesh')==unreal.load_asset(ARROW)
    assert abs(mesh.get_editor_property('relative_rotation').yaw+90.) < .001
    assert mesh.get_editor_property('relative_scale3d')==unreal.Vector(1,1,1)
    assert mesh.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    selected = [r for r in rows() if r['SkillID'] in IDS]
    assert any(r['SkillID']==15102 for r in selected)
    for row in selected:
        for key, expected in skill_patch().items():
            assert abs(row[key]-expected)<1.e-6 if isinstance(expected,float) else row[key]==expected, (row['SkillID'],key,row[key])
        assert row['Pattern']=='AimedProjectile'
    assert all(r['Desc']=='별빛 정밀 사격' for r in selected if r['SkillID']==15112)
    unreal.log('PGSkeletonArcher VALIDATION PASS')


def apply():
    before = rows()
    data = copy.deepcopy(before)
    backup = ROOT/'Saved/Backups/SkeletonArcher'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    for path in [TABLE,MONTAGE,PROJECTILE]:
        relative=path.removeprefix('/Game/')+'.uasset'
        source,target=ROOT/'Content'/relative,backup/relative
        if source.exists():
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(source,target)
    backup.mkdir(parents=True,exist_ok=True)
    (backup/'skills.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
    montage=unreal.load_asset(MONTAGE) if EAL.does_asset_exist(MONTAGE) else EAL.duplicate_asset(SOURCE,MONTAGE)
    assert montage
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(montage)
    save(montage)
    if EAL.does_asset_exist(PROJECTILE):
        blueprint=unreal.load_asset(PROJECTILE)
    else:
        factory=unreal.BlueprintFactory()
        factory.set_editor_property('parent_class',unreal.PGPatternProjectile)
        blueprint=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_PGSkeletonArrow',DEST,unreal.Blueprint,factory)
    cdo=unreal.get_default_object(blueprint.generated_class())
    mesh=cdo.get_editor_property('mesh_component')
    mesh.set_editor_property('static_mesh',unreal.load_asset(ARROW))
    # The existing arrow points along +Y; projectile movement faces +X.
    mesh.set_editor_property('relative_rotation',unreal.Rotator(yaw=-90.))
    mesh.set_editor_property('relative_scale3d',unreal.Vector(1,1,1))
    mesh.set_editor_property('override_materials',[])
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)
    for row in data:
        if row['SkillID'] in IDS:
            row.update(skill_patch())
            if row['SkillID']==15112: row['Desc']='별빛 정밀 사격'
    assert [r for r in before if r['SkillID'] not in IDS]==[r for r in data if r['SkillID'] not in IDS]
    table=unreal.load_asset(TABLE)
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(data,ensure_ascii=False))
    save(table)
    validate()
    unreal.log('PGSkeletonArcher APPLY PASS backup='+str(backup))


if __name__=='__main__':
    validate() if '-PGArcherValidate' in unreal.SystemLibrary.get_command_line() else apply()
