"""Back up and change only the existing stage table's chaser spawn composition."""
from datetime import datetime,timezone
import json
from pathlib import Path
import shutil
import sys
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from P09WaveRoster import compose, validate_roster, P09_IDS
TABLE='/Game/DataCenter/DataTables/Stage/DT_StageData'


def rows(path):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))


def validate(stages=None):
    stages=rows(TABLE) if stages is None else stages
    enemies={r['EnemyID']:r for r in rows('/Game/DataCenter/DataTables/Actor/DT_Enemy')}
    stats={r['CharacterID']:r for r in rows('/Game/DataCenter/DataTables/Actor/DT_CharacterStat')}
    skills={r['SkillID']:r for r in rows('/Game/DataCenter/DataTables/Skill/DT_Skill')}
    for eid in P09_IDS:
        row=enemies[eid]
        assert eid in stats and row['Role']=='Chaser' and row['SkillIdList'] and row['CombatBehaviorTree']!='None'
        assert all(sid in skills for sid in row['SkillIdList'])
        cdo=unreal.get_default_object(unreal.load_class(None,row['ActorClass']))
        assert cdo.get_editor_property('character_tid')==eid
        assert cdo.appearance_component.get_editor_property('default_appearance')
        assert cdo.get_editor_property('ai_controller_class')==unreal.PGRoleAIController.static_class()
    from ValidateLootPools import validate_loot_pools
    validate_loot_pools(unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression'),enemies)
    validate_roster(stages)
    from ValidateContentMilestone import validate_content_milestone
    validate_content_milestone(enemies,skills,stages)
    unreal.log('PGP09Waves VALIDATION PASS')


def apply():
    before=rows(TABLE)
    after=compose(before)
    validate(after)
    backup=ROOT/'Saved/Backups/P09Waves'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup.mkdir(parents=True)
    shutil.copy2(ROOT/'Content/DataCenter/DataTables/Stage/DT_StageData.uasset',backup/'DT_StageData.uasset')
    (backup/'stages_before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
    (backup/'stages_after.json').write_text(json.dumps(after,ensure_ascii=False,indent=2),encoding='utf-8')
    if before!=after:
        table=unreal.load_asset(TABLE)
        assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(after,ensure_ascii=False))
        assert unreal.EditorAssetLibrary.save_loaded_asset(table,only_if_is_dirty=False)
    assert rows(TABLE)==after
    unreal.log('PGP09Waves APPLY PASS backup='+str(backup))


if __name__=='__main__':
    validate() if '-PGP09WavesValidate' in unreal.SystemLibrary.get_command_line() else apply()
