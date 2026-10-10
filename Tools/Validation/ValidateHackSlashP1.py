"""Saved asset checks plus the chained P0-to-P1 non-target baseline."""
import json
from pathlib import Path
import unreal

def validate_hack_slash_p1(rows):
    root=Path(unreal.Paths.project_dir())
    spec=json.loads((root/'Tools/Validation/HackSlashP1.json').read_text(encoding='utf-8'))
    by_id={r['SkillID']:r for r in rows}
    baseline=json.loads((root/'Tools/Validation/Baselines/HackSlashP1_Skills.json').read_text(encoding='utf-8'))
    from MonsterVariationRoster import SPEC as monster_spec
    additional={s['id'] for s in monster_spec['skills']}
    boss_spec=json.loads((root/'Tools/Validation/Data/HumanoidBoss.json').read_text(encoding='utf-8'))
    additional.update(s['id'] for s in boss_spec['skills'])
    from MonsterVariationRoster import CREATURE_COMBAT
    additional.update(s['id'] for s in CREATURE_COMBAT['skills'])
    dark_knight=json.loads((root/'Tools/Validation/Data/DarkKnightBoss.json').read_text(encoding='utf-8'))
    additional.update(s['id'] for s in dark_knight['skills'])
    assert len(rows)==len(by_id) and set(by_id)-{r['SkillID'] for r in baseline} <= additional
    # Later migrations own only these fields; validate their current contracts
    # instead of comparing them against an obsolete P1 snapshot.
    overrides={}
    from ConfigurePlayerDash import MONTAGE as dash_montage, path_ref, validate as validate_dash
    if unreal.EditorAssetLibrary.does_asset_exist(dash_montage):
        validate_dash()
        overrides[10000]=dict(Desc='대시',MontagePath=path_ref(dash_montage),
            SkillIconPath=path_ref('/Game/UI/SkillIcons/T_Skill_Dash'))
    from ConfigureSkeletonArcher import MONTAGE as bow_montage, IDS as bow_ids, skill_patch, validate as validate_archer
    if unreal.EditorAssetLibrary.does_asset_exist(bow_montage):
        validate_archer()
        overrides.update({sid:skill_patch() for sid in bow_ids})
        overrides[15112]['Desc']='별빛 정밀 사격'
    for old in baseline:
        for key,value in old.items():
            if old['SkillID'] in (110,113,114) and key in ('PlayerProfile','SkillCoolTime','Desc'): continue
            expected=overrides.get(old['SkillID'],{}).get(key,value)
            actual=by_id[old['SkillID']][key]
            assert abs(actual-expected)<1.e-6 if isinstance(expected,float) else actual==expected,(old['SkillID'],key)
    for item in spec['skills']:
        row=by_id[item['id']]
        assert row['PlayerProfile'].startswith('/Game/DataCenter/HackSlashP1/')
        p=unreal.load_asset(row['PlayerProfile']); assert p
        assert row['SkillCoolTime']==item['cooldown'] and row['Desc']==item['name']
        for key,value in [('skill_id',item['id']),('duration',item['duration']),('aim_lock',item['aim']),('dodge_cancel',item['dodge']),('attack_cancel',item['attack']),('early_dodge_until',item['early_dodge'])]:
            assert abs(p.get_editor_property(key)-value)<.0001,(item['id'],key)
        phases=p.get_editor_property('hit_phases'); assert len(phases)==len(item['hits'])
        for i,phase in enumerate(phases):
            for key,value in [('phase_id',i),('start',item['hits'][i]),('damage_multiplier',item['damage'][i]),('radius',item['radius']),('full_angle_degrees',item['angle'])]:
                assert abs(phase.get_editor_property(key)-value)<.0001,(item['id'],key)
            assert phase.get_editor_property('shape')==getattr(unreal.PGPlayerHitShape,item['shape'])
            assert phase.get_editor_property('proc_policy').get_editor_property('bleed_burst')==(i==len(phases)-1)
        moves=p.get_editor_property('movement_segments'); assert len(moves)==len(item['moves'])
        for move,(start,end,distance,mode) in zip(moves,item['moves']):
            for key,value in [('start',start),('end',end),('distance',distance)]: assert abs(move.get_editor_property(key)-value)<.0001
            assert move.get_editor_property('mode')==getattr(unreal.PGPlayerMoveMode,mode)
        montage=unreal.load_asset(row['MontagePath']['AssetPath']['PackageName'])
        from ConfigureAttackMotion import validate_profile_motion
        validate_profile_motion(item, p, montage)
        assert p.get_editor_property('pose_keys')[-1].get_editor_property('montage_seconds')<=montage.get_play_length()+.0001
        if item['id']==114:
            for key,value in [('projectile_speed',1800),('projectile_range',1000),('projectile_lifetime',.7)]: assert abs(p.get_editor_property(key)-value)<.0001
    catalog=unreal.load_asset('/Game/DataCenter/Progression/DA_PGProgression')
    assert list(catalog.get_editor_property('selectable_active_skills'))==[110,111,112,113,114]
    assert list(catalog.get_editor_property('default_active_skills'))==[111,112,110,114]
    unreal.log('PGHackSlashP1 VALIDATION PASS profiles=3 selectable=5 non_target_rows_unchanged=1')
    return {r['SkillID']:r for r in baseline}
