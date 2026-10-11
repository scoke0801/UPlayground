"""Narrow, backed-up profile patch; all damage/contact windows remain unchanged."""
import json
import os
from pathlib import Path
import sys
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Tools/Validation'))
from PlayableCharacterTransaction import Transaction, write_json
OUT=Path(os.environ['PG_BOSS_TEMPO_RUN'])
STEP=os.environ['PG_BOSS_TEMPO_STEP']
dark=json.loads((ROOT/'Tools/Validation/Data/DarkKnightBoss.json').read_text(encoding='utf-8'))
human=json.loads((ROOT/'Tools/Validation/Data/HumanoidBoss.json').read_text(encoding='utf-8'))
double=next(x for x in dark['skills'] if x['id']==15613)
combo=next(x for x in human['skills'] if x['id']==15401)
thrust=next(x for x in human['skills'] if x['id']==15402)
changes={
    '/Game/DataCenter/DarkKnightBoss/DA_Skill_15613':(1,{'motion_start':double['contacts'][1]['start']}),
    '/Game/DataCenter/HumanoidBoss/DA_Combo':(1,{'end_fraction':combo['final_end_fraction']}),
    '/Game/DataCenter/HumanoidBoss/DA_Thrust':(0,{k:thrust[k] for k in ('swing_start_fraction','swing_seconds')})}
fields='motion_start time shape radius half_angle length half_width inner_radius damage_multiplier bHeavyImpact montage start_fraction contact_fraction end_fraction swing_start_fraction swing_seconds'.split()

def snapshot(asset):
    return {prop:[{k:(h.get_editor_property(k).get_path_name() if k=='montage' else str(h.get_editor_property(k))) for k in fields} for h in asset.get_editor_property(prop)]
            for prop in ('contacts','phase_two_contacts')}

assets={p:unreal.load_asset(p) for p in changes}
assert all(assets.values()),'Missing boss profiles'
enemy_path='/Game/DataCenter/DataTables/Actor/DT_Enemy'
enemy_table=unreal.load_asset(enemy_path)
enemies=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(enemy_table))
if STEP=='apply':
    tx=Transaction(ROOT,OUT);tx.prepare(list(changes)+[enemy_path],ROOT/'Tools/Validation/Data/HumanoidBoss.json')
    write_json(OUT/'before.json',{p:snapshot(a) for p,a in assets.items()})
    write_json(OUT/'enemies-before.json',enemies)
    for p,a in assets.items():
        index,values=changes[p];contacts=list(a.get_editor_property('contacts'))
        for k,v in values.items():contacts[index].set_editor_property(k,v)
        a.set_editor_property('contacts',contacts)
        tx.mark_written(p)
        assert unreal.EditorAssetLibrary.save_loaded_asset(a,only_if_is_dirty=False)
    next(r for r in enemies if r['EnemyID']==15401)['bPrioritizeGuard']=True
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(enemy_table,json.dumps(enemies,ensure_ascii=False))
    tx.mark_written(enemy_path)
    assert unreal.EditorAssetLibrary.save_loaded_asset(enemy_table,only_if_is_dirty=False)
    tx.data['status']='APPLIED_UNVERIFIED';tx.flush()
before=json.loads((OUT/'before.json').read_text(encoding='utf-8'))
for p,a in assets.items():
    index,values=changes[p];after=snapshot(a)
    for k,v in values.items():
        assert abs(float(after['contacts'][index][k])-v)<1.e-5,(p,k)
        before[p]['contacts'][index][k]=after['contacts'][index][k]
    assert before[p]==after,('Unrelated contact or phase changed',p)
enemies=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(enemy_table))
expected=json.loads((OUT/'enemies-before.json').read_text(encoding='utf-8'))
next(r for r in expected if r['EnemyID']==15401)['bPrioritizeGuard']=True
assert enemies==expected,'Unrelated enemy settings changed'
write_json(OUT/'validation.json',dict(status='PASS',profiles=list(changes),damage_and_contact_windows_preserved=True,reactive_guard_priority=True))
unreal.log('PGBossTempo '+STEP.upper()+' PASS')
