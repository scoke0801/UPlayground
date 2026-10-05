"""Author the seven selectable toon identities and four extensible P09 enemy templates.

Run using UE 5.8 Python. Backs up every existing package before saving; source art,
source animations and arena wave compositions are preserved.
"""
import copy
import json
import traceback
import os
import sys
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from PlayableCharacterPolish import (read_source, preflight, packages, snapshot, digest, equivalent, STAMP,
    apply_rig, apply_retarget, apply_appearance, path as object_path)
from PlayableCharacterTransaction import Transaction
OUT=ROOT/'Saved/PlayableCharacters'
if 'PG_CHARACTER_RUN' not in os.environ:
    raise RuntimeError('Use RunPlayableCharacters.py --step configure for rollback and reload verification')
RUN=Path(os.environ['PG_CHARACTER_RUN'])
RUN.mkdir(parents=True, exist_ok=True)
DEST='/Game/DataCenter/Characters'
EAL=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
REPORT=dict(status='RUNNING',run=str(RUN),players=[],enemies=[])
POLISH=read_source()
TRANSACTION=Transaction(ROOT, RUN)

def preserve(path):
    package=path.split('.')[0]
    assert package in TRANSACTION.data['packages'], 'Unplanned package: '+package

def save(asset):
    preserve(asset.get_path_name())
    package=asset.get_path_name().split('.')[0]
    if package in POLISH['assets']:
        actual=snapshot(asset)
        assert equivalent(actual, POLISH['assets'][package]), 'Source not reproduced: '+package
        EAL.set_metadata_tag(asset, STAMP, digest(actual))
    TRANSACTION.mark_written(package)
    assert EAL.save_loaded_asset(asset,only_if_is_dirty=False),asset.get_path_name()
    fail_after=int(os.environ.get('PG_CHARACTER_FAIL_AFTER_SAVE','0'))
    REPORT['saved_count']=REPORT.get('saved_count',0)+1
    if fail_after and REPORT['saved_count']==fail_after:
        raise RuntimeError('Injected package save failure')

def own(name,cls,factory):
    path=DEST+'/'+name
    preserve(path)
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(name,DEST,cls,factory)

def data(name):
    factory=unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class',unreal.PGCharacterAppearance)
    return own(name,unreal.PGCharacterAppearance,factory)

def appearance(identity):
    # These packages are outputs. Replay the exported source instead of inferring
    # chains/auto-aligning over an artist's calibrated pose on every regeneration.
    for suffix in ('_Source','_Target'):
        name='IK_'+identity+suffix
        asset=own(name,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
        apply_rig(asset, POLISH['assets'][DEST+'/'+name])
        save(asset)
    name='RTG_'+identity
    asset=own(name,unreal.IKRetargeter,unreal.IKRetargetFactory())
    apply_retarget(asset, POLISH['assets'][DEST+'/'+name])
    save(asset)
    asset=data('DA_'+identity)
    apply_appearance(asset, POLISH['assets'][DEST+'/DA_'+identity])
    save(asset)
    return asset

def table_rows(path):
    table=unreal.load_asset(path)
    return table,json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))

def main():
    preflight(POLISH)
    extra=[DEST+'/BP_PGEnemy_P09_'+label for label in ('Female','Male','Female_Armor007','Male_Armor007')]
    extra+=['/Game/DataCenter/Progression/DA_PGProgression','/Game/Blueprints/Actor/LocalPlayer/BP_LocalPlayer',
            '/Game/DataCenter/DataTables/Actor/DT_Enemy','/Game/DataCenter/DataTables/Path/DT_Death']
    # Resolve the actual stat-table path before ANY writes, including rig creation.
    for entry in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/DataCenter/DataTables',recursive=True):
        if str(entry.asset_class_path.asset_name)!='DataTable': continue
        table,contents=table_rows(str(entry.package_name))
        if contents and 'CharacterID' in contents[0] and any(r['CharacterID']==15101 for r in contents):
            extra.append(str(entry.package_name)); break
    assert len(extra)==9, 'Missing enemy stat table'
    REPORT['planned_packages']=packages()+extra
    REPORT['preserved_portraits']={p:object_path(unreal.load_asset(p).get_editor_property('portrait'))
        for p in packages() if '/DA_' in p and EAL.does_asset_exist(p)}
    unreal.log('PG polish planned packages: '+json.dumps(REPORT['planned_packages']))
    TRANSACTION.prepare(REPORT['planned_packages'], ROOT/'Tools/Validation/Data/PlayableCharacterPolish.json')
    players=[]
    for identity in ['Bokusei','LianLian','Honoka','Hichi','Siuha','Lili','Nenmir']:
        asset=appearance(identity)
        meshpath=asset.get_editor_property('mesh').get_path_name()
        players.append(asset)
        REPORT['players'].append(dict(id=identity,asset=asset.get_path_name(),mesh=meshpath))
    catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
    preserve(catalog.get_path_name())
    catalog.set_editor_property('playable_characters',players)
    save(catalog)
    player_bp=unreal.load_asset('/Game/Blueprints/Actor/LocalPlayer/BP_LocalPlayer')
    preserve(player_bp.get_path_name())
    camera=unreal.get_default_object(player_bp.generated_class()).get_component_by_class(unreal.CameraComponent)
    assert camera
    camera.add_or_update_blendable(unreal.load_asset('/Game/Art/ToonTest/Advanced/Materials/M_PGToonScreenOutline'),1.)
    save(player_bp)
    enemies,rows=table_rows('/Game/DataCenter/DataTables/Actor/DT_Enemy')
    for row in rows:
        if row['EnemyID'] in range(15201,15205):
            assert row['ActorClass'].startswith(DEST+'/BP_PGEnemy_P09_'),'Enemy ID already belongs to another asset: '+str(row['EnemyID'])
    death_table,death_rows=table_rows('/Game/DataCenter/DataTables/Path/DT_Death')
    death_template=next((r for r in death_rows if r['ObjectTID']==15101),None)
    template=next(r for r in rows if r['EnemyID']==15101)
    parent=unreal.load_asset(template['ActorClass'].split('.')[0])
    stat_assets=[a for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/DataCenter/DataTables',recursive=True) if str(a.asset_class_path.asset_name)=='DataTable']
    stat_table=stat_rows=None
    for entry in stat_assets:
        candidate,candidate_rows=table_rows(str(entry.package_name))
        if candidate_rows and 'CharacterID' in candidate_rows[0] and any(r['CharacterID']==15101 for r in candidate_rows):
            stat_table,stat_rows=candidate,candidate_rows
            break
    assert stat_table,'Missing enemy stat table'
    stat_template=next(r for r in stat_rows if r['CharacterID']==15101)
    REPORT['preserved_gameplay_rows']={table.get_path_name():copy.deepcopy(contents)
        for table,contents in ((enemies,rows),(stat_table,stat_rows),(death_table,death_rows))}
    for index,label in enumerate(('Female','Male','Female_Armor007','Male_Armor007')):
        asset=appearance('P09_'+label)
        names=[part.mesh.get_path_name() for part in asset.get_editor_property('parts')]
        factory=unreal.BlueprintFactory()
        factory.set_editor_property('parent_class',parent.generated_class())
        bp=own('BP_PGEnemy_P09_'+label,unreal.Blueprint,factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo=unreal.get_default_object(bp.generated_class())
        eid=15201+index
        cdo.set_editor_property('character_tid',eid)
        cdo.appearance_component.set_editor_property('default_appearance',asset)
        save(bp)
        row=copy.deepcopy(template)
        row.update(Name=str(eid),EnemyID=eid,EnemyName='P09 '+label,ActorClass=bp.generated_class().get_path_name())
        # Existing enemy balance/death rows belong to gameplay authoring. Only seed
        # missing rows; cosmetic regeneration must not reset a tuned enemy template.
        if not any(r['EnemyID']==eid for r in rows): rows.append(row)
        stat=copy.deepcopy(stat_template)
        stat.update(Name=str(eid),CharacterID=eid)
        if not any(r['CharacterID']==eid for r in stat_rows): stat_rows.append(stat)
        if death_template:
            death=copy.deepcopy(death_template)
            death.update(Name=str(eid),ObjectTID=eid)
            if not any(r['ObjectTID']==eid for r in death_rows): death_rows.append(death)
        REPORT['enemies'].append(dict(id=eid,asset=asset.get_path_name(),blueprint=bp.get_path_name(),parts=names,template_id=15101))
    # UE JSON exports numeric map values wrapped in the map property's name.
    for stat in stat_rows:
        stat['Stats']={k:(v.get('Stats',0) if isinstance(v,dict) else v) for k,v in stat['Stats'].items()}
    for table,contents in ((enemies,rows),(stat_table,stat_rows),(death_table,death_rows)):
        preserve(table.get_path_name())
        assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(contents,ensure_ascii=False))
        save(table)
    REPORT['status']='APPLIED'
    TRANSACTION.data['status']='APPLIED_UNVERIFIED'
    TRANSACTION.flush()

try: main()
except Exception:
    REPORT.update(status='FAIL',error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    payload=json.dumps(REPORT,ensure_ascii=False,indent=2)
    (RUN/'configure.json').write_text(payload,encoding='utf-8')
    (OUT/'configure.json').write_text(payload,encoding='utf-8')
if REPORT['status']!='APPLIED': raise RuntimeError('Playable character configuration failed')
