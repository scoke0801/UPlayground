"""Validate saved monster kits, readable timing and matching telegraph parameters."""
import json
import math
from pathlib import Path
import unreal


def validate_combat_variety(enemies, skills):
    spec = json.loads((Path(unreal.Paths.project_dir()) / 'Tools/Validation/CombatVariety.json').read_text(encoding='utf-8'))
    material_path = '/Game/DataCenter/CombatVariety/M_PGCombatVarietyBounds'
    material = unreal.load_asset(material_path)
    assert material, 'Missing shared telegraph material'
    scalars = {str(n) for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(material)}
    assert {'Radius','Length','HalfWidth','Shape','CosAngle','InnerRadius','VolleyCount','SpreadRadians'} <= scalars
    ids = set()
    for eid, kit in spec['enemy_kits'].items():
        assert enemies[int(eid)]['SkillIdList'] == kit, eid
        ids.update(kit)
        assert len({skills[sid]['Pattern'] for sid in kit}) >= 2 or int(eid) == 15102
    assert enemies[15106]['PhaseTwoSkillSequence'] == spec['boss_sequence']
    for sid in ids:
        row = skills[sid]
        for field in ('TelegraphDuration','RecoveryDuration','SkillRange','SelectionWeight'):
            assert math.isfinite(row[field]) and row[field] > 0, (sid,field)
        assert 0 <= row['AimTrackingSeconds'] <= row['TelegraphDuration'] - .15, sid
        assert 0 <= row['MinimumActivationRange'] < row['SkillRange'], sid
        assert 1 <= row['AttackPressureCost'] <= 3
        assert row['TelegraphMaterial'].split('.')[0] == material_path
        assert unreal.load_asset(row['ElitePresentationMontage']), sid
        if row['Pattern'] == 'RingBurst': assert 100 <= row['InnerSafeRadius'] < row['TelegraphRadius']
        if row['Pattern'] == 'Thrust': assert row['SkillRange'] <= row['TravelDistance'] and row['LineHalfWidth'] >= 40
    for definition in spec['new_skills']:
        row = skills[definition['SkillID']]
        for key,value in definition.items():
            if key != 'base':
                assert math.isclose(row[key],value,rel_tol=1e-5) if isinstance(value,(int,float)) else row[key] == value, (definition['SkillID'],key)
    assert skills[15112]['ProjectileCount'] == 3 and skills[15112]['ProjectileSpreadHalfAngle'] >= 20
    assert skills[15109]['MinimumBossPhase'] == 2 and skills[15109]['RecoveryDuration'] >= 1.5
    result = dict(schema=1,enemies=6,skills=len(ids),new_skills=len(spec['new_skills']),boss_attacks=4)
    unreal.log('PGCombatVariety VALIDATION PASS ' + json.dumps(result))
    return result
