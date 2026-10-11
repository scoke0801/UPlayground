"""Patch only six saved skill rows, preserving all combat windows and other assets."""
import copy
import json
import os
from pathlib import Path
import sys
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from ConfigureMonsterVariations import TABLES, rows
from P09WeaponRoster import SPEC, SPEC_PATH
from PlayableCharacterTransaction import Transaction, write_json

OUT=Path(os.environ['PG_ENEMY_TEMPO_RUN'])
STEP=os.environ['PG_ENEMY_TEMPO_STEP']
definitions={d['id']:d for d in SPEC['skills'] if 'tempo' in d}
data=rows(TABLES['skills'])
before=copy.deepcopy(data)
if STEP=='apply':
    tx=Transaction(ROOT,OUT)
    tx.prepare([TABLES['skills']],SPEC_PATH)
    write_json(OUT/'before.json',before)
    for row in data:
        if row['SkillID'] in definitions:
            d=definitions[row['SkillID']]
            row.update(MotionTempo=d['tempo'],AimTurnRate=d['aim_turn_rate'])
    table=unreal.load_asset(TABLES['skills'])
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(data,ensure_ascii=False))
    tx.mark_written(TABLES['skills'])
    assert unreal.EditorAssetLibrary.save_loaded_asset(table,only_if_is_dirty=False)
    tx.data['status']='APPLIED_UNVERIFIED';tx.flush()
    data=rows(TABLES['skills'])

previous={r['SkillID']:r for r in json.loads((OUT/'before.json').read_text(encoding='utf-8'))} if (OUT/'before.json').exists() else None
assert set(definitions)<={r['SkillID'] for r in data}
if previous:assert set(previous)=={r['SkillID'] for r in data},'skill rows removed or added'
for row in data:
    sid=row['SkillID']
    if sid in definitions:
        d=definitions[sid]
        for key,value in d['tempo'].items():assert abs(row['MotionTempo'][key]-value)<.0001,(sid,key)
        assert abs(row['AimTurnRate']-d['aim_turn_rate'])<.0001
        assert row['bSyncMontageToPattern'] and row['EnemyProfile']=='None' and row['Pattern'] in ('Sweep','Thrust')
        t=row['MotionTempo']
        assert row['WindupMontageFraction']<t['SwingStartFraction']<row['ImpactMontageFraction']<t['SettleStartFraction']<1
        assert .05<=t['SwingSeconds']<=row['TelegraphDuration']-.05
        assert .05<=t['FollowThroughSeconds']<=row['RecoveryDuration']-.05
    if previous:
        old=copy.deepcopy(previous[sid]);current=copy.deepcopy(row)
        if sid in definitions:
            for key in ('MotionTempo','AimTurnRate'):old.pop(key,None);current.pop(key,None)
        assert old==current,('unrelated skill data changed',sid)
write_json(OUT/'validation.json',dict(status='PASS',skills=sorted(definitions),unrelated_rows_preserved=previous is not None))
unreal.log('PGEnemyTempo '+STEP.upper()+' PASS')
