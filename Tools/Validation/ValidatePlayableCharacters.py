"""Fresh-process validation of playable and P09 references; no asset writes."""
import json
import traceback
import sys
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from PlayableCharacterCatalog import PLAYER_IDS, MODEL_REPLACEMENTS
OUT=ROOT/'Saved/PlayableCharacters'
report=dict(status='RUNNING',players=[],enemies=[])
try:
    catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
    identities=[]
    for reference in catalog.get_editor_property('playable_characters'):
        asset=unreal.load_asset(str(reference)) if not isinstance(reference,unreal.PGCharacterAppearance) else reference
        assert asset
        identity=str(asset.get_editor_property('id'))
        identities.append(identity)
        mesh=asset.get_editor_property('mesh')
        source=asset.get_editor_property('source_mesh')
        retarget=unreal.load_asset('/Game/DataCenter/Characters/RTG_'+identity)
        assert unreal.SystemLibrary.get_object_from_soft_path(asset.get_editor_property('retargeter'))==retarget,identity
        assert mesh and source and retarget,identity
        ctl=unreal.IKRetargeterController.get_controller(retarget)
        s,t=unreal.RetargetSourceOrTarget.SOURCE,unreal.RetargetSourceOrTarget.TARGET
        assert ctl.get_preview_mesh(s)==source and ctl.get_preview_mesh(t)==mesh,identity
        assert len(asset.get_editor_property('equipment_bones'))>=10,identity
        assert bool(asset.get_editor_property('reconstruct_scaled_translations')) == (identity in MODEL_REPLACEMENTS),identity
        report['players'].append(dict(id=identity,mesh=mesh.get_path_name()))
    assert identities==PLAYER_IDS,identities
    for identity,(_,name) in MODEL_REPLACEMENTS.items():
        asset=unreal.load_asset('/Game/DataCenter/Characters/DA_'+identity)
        assert str(asset.get_editor_property('display_name'))==name
        assert asset.get_editor_property('mesh').get_path_name().split('.')[0]=='/Game/Art/PlayerModels/'+name+'/SK_PG_'+name
        assert asset.get_editor_property('portrait'),identity
    player=unreal.get_default_object(unreal.load_asset('/Game/Blueprints/Actor/LocalPlayer/BP_LocalPlayer').generated_class())
    camera=player.get_component_by_class(unreal.CameraComponent)
    blendables=camera.get_editor_property('post_process_settings').weighted_blendables.array
    assert any(b.object and 'M_PGToonScreenOutline' in b.object.get_path_name() and b.weight>0 for b in blendables),'Gameplay camera outline missing'
    rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/DataCenter/DataTables/Actor/DT_Enemy')))
    stats=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/DataCenter/DataTables/Actor/DT_CharacterStat')))
    deaths=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/DataCenter/DataTables/Path/DT_Death')))
    for eid in range(15201,15205):
        row=next(r for r in rows if r['EnemyID']==eid)
        assert row['Role']=='Chaser' and row['SkillIdList'] and row['CombatBehaviorTree'],row
        bp=unreal.load_asset(row['ActorClass'].split('.')[0])
        cdo=unreal.get_default_object(bp.generated_class())
        assert cdo.get_editor_property('character_tid')==eid
        if row['ActorClass'].startswith('/Game/DataCenter/MonsterVariations/'):
            from MonsterVariationRoster import SPEC
            grade=next(g for g in SPEC['p09_grades'] if eid in g['ids'])
            actual=next(r for r in stats if r['CharacterID']==eid)['Stats']
            assert all((actual[k]['Stats'] if isinstance(actual[k],dict) else actual[k])==v for k,v in grade['stats'].items())
        else:
            assert next(r for r in stats if r['CharacterID']==eid)['Stats']==next(r for r in stats if r['CharacterID']==15101)['Stats']
        source_death=next((r for r in deaths if r['ObjectTID']==15101),None)
        if source_death: assert next(r for r in deaths if r['ObjectTID']==eid)['DeathMontagePath']==source_death['DeathMontagePath']
        asset=cdo.appearance_component.get_editor_property('default_appearance')
        assert asset and asset.get_editor_property('parts')
        assert asset.get_editor_property('reconstruct_scaled_translations')
        mesh=asset.get_editor_property('mesh')
        for part in asset.get_editor_property('parts'):
            assert part.mesh and (str(part.attach_bone)!='None' or part.mesh.skeleton==mesh.skeleton)
        report['enemies'].append(dict(id=eid,blueprint=bp.get_path_name(),parts=len(asset.get_editor_property('parts'))))
    report['status']='PASS'
except Exception:
    report.update(status='FAIL',error=traceback.format_exc())
    unreal.log_error(report['error'])
(OUT/'validate.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
if report['status']!='PASS': raise RuntimeError('PlayableCharacters validation failed')
