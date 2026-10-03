"""Read-only checks for the authored enemy-role migration (content schema 1)."""
import json
import math
import unreal


def validate_content_milestone(enemies, skills, stages):
    variety = unreal.EditorAssetLibrary.does_asset_exist('/Game/DataCenter/CombatVariety/M_PGCombatVarietyBounds')
    roles = {15101: ('Chaser', 'Sweep'), 15102: ('Shooter', 'AimedProjectile'),
             15103: ('Guardian', 'Sweep'), 15104: ('Crusher', 'ChargeSlam'),
             15105: ('Warden', 'HazardSequence')}
    if all(enemies[eid].get('Role', 'Legacy') == 'Legacy' for eid in roles):
        assert not unreal.EditorAssetLibrary.does_asset_exist('/Game/DataCenter/ContentMilestone/M_PatternBounds'), 'Migrated roles unexpectedly reverted to Legacy'
        return 0  # Legacy assets remain valid until the explicit migration is applied.
    for eid, (role, pattern) in roles.items():
        enemy, skill = enemies[eid], skills[eid]
        expected_kit = [eid, eid+10] + ([15116] if eid == 15104 else []) if variety else [eid]
        assert enemy['Role'] == role and enemy['SkillIdList'] == expected_kit, eid
        assert skill['Pattern'] == pattern and skill['MinimumBossPhase'] == 1, eid
        for name in ('TelegraphDuration', 'TelegraphRadius', 'RecoveryDuration', 'TravelSpeed',
                     'TravelDistance', 'LineHalfWidth', 'LandingTelegraphSeconds', 'SkillRange'):
            assert math.isfinite(skill[name]) and skill[name] > 0, (eid, name)
        assert 0 <= skill['AimTrackingSeconds'] < skill['TelegraphDuration'], eid
        assert 0 < skill['HalfAngleDegrees'] <= 180 and 1 <= skill['HazardCount'] <= 8, eid
        assert skill['HazardInterval'] >= .1 and skill['HazardSpacing'] >= 0, eid
        for name in ('TelegraphMaterial', 'ElitePresentationMontage', 'SlamVFX', 'AttackSound'):
            assert unreal.load_asset(skill[name]), (eid, name)
        cls = unreal.load_class(None, enemy['ActorClass'])
        controller = unreal.get_default_object(cls).get_editor_property('ai_controller_class')
        assert controller == unreal.PGRoleAIController.static_class(), (eid, str(controller))
    assert unreal.load_class(None, skills[15102]['ProjectileClass']) == unreal.PGPatternProjectile.static_class()
    assert 0 < enemies[15103]['GuardReduction'] < 1 and 0 < enemies[15103]['GuardHalfAngle'] < 180
    assert enemies[15103]['TurnSpeed'] < enemies[15101]['TurnSpeed']
    assert enemies[15102]['PreferredDistance'] + enemies[15102]['DistanceTolerance'] < skills[15102]['SkillRange']
    # Versioned authored counts, independent of observed runtime values.
    expected = {1: [5, 6, 4], 2: [6, 7, 6], 3: [7, 8, 5], 4: [8, 9, 7], 5: [9, 10, 6]}
    by_id = {row['Id']: row for row in stages}
    for sid, counts in expected.items():
        assert [sum(s['SpawnCount'] for s in w['MonsterSpawnInfos']) for w in by_id[sid]['Waves']] == counts, sid
    wave_ids = lambda sid: {s['MonsterId'] for w in by_id[sid]['Waves'] for s in w['MonsterSpawnInfos']}
    assert wave_ids(1) == {15101, 15102}
    assert {15103, 15104} <= wave_ids(2) and 15105 not in wave_ids(2)
    assert 15105 in wave_ids(4) and set(roles) == wave_ids(5)
    schema = 1
    boss = enemies[15106]
    if boss.get('Role') == 'Boss':
        schema = 2
        assert boss['SkillIdList'] == [15106,15107,15108] + ([15109] if variety else [])
        assert boss['PhaseTwoSkillSequence'] == [15108,15107,15106] + ([15109] if variety else [])
        assert 0 < boss['PhaseTwoHealthRatio'] < 1 and 0 <= boss['PhaseTransitionSeconds'] <= 5
        assert 0 < boss['DefeatDisplaySeconds'] <= 10
        for field in ('PhaseVFX','PhaseSound','DefeatVFX','DefeatSound'):
            assert unreal.load_asset(boss[field]), field
        cls = unreal.load_class(None, boss['ActorClass'])
        assert unreal.get_default_object(cls).get_editor_property('ai_controller_class') == unreal.PGRoleAIController.static_class()
        for sid, pattern in ((15106,'Sweep'), (15107,'ChargeSlam'), (15108,'HazardSequence')):
            skill = skills[sid]
            assert skill['Pattern'] == pattern and skill['MinimumBossPhase'] == (2 if sid == 15108 else 1), sid
            for field in ('TelegraphDuration','TelegraphRadius','RecoveryDuration','SkillCoolTime','TravelSpeed',
                          'TravelDistance','LineHalfWidth','LandingTelegraphSeconds','SkillRange'):
                assert math.isfinite(skill[field]) and skill[field] > 0, (sid,field)
            assert 0 <= skill['AimTrackingSeconds'] < skill['TelegraphDuration']
            assert 0 < skill['HalfAngleDegrees'] <= 180 and 1 <= skill['HazardCount'] <= 8
            assert skill['HazardInterval'] >= .1 and skill['HazardSpacing'] >= 0
            for field in ('TelegraphMaterial','ElitePresentationMontage','SlamVFX','AttackSound'):
                assert unreal.load_asset(skill[field]), (sid,field)
        assert by_id[6]['bIsBossStage'] and len(by_id[6]['Waves']) == 1
        assert [(s['MonsterId'], s['SpawnCount']) for s in by_id[6]['Waves'][0]['MonsterSpawnInfos']] == [(15106,1)]
    else:
        assert 15107 not in skills and 15108 not in skills, 'Partial boss migration'
    if variety:
        from ValidateCombatVariety import validate_combat_variety
        validate_combat_variety(enemies, skills)
        schema = 3
    unreal.log('PGContent VALIDATION PASS ' + json.dumps(dict(schema=schema, roles=5, boss_attacks=4 if variety else 3 if schema==2 else 0, waves=15, wave_counts=expected)))
    return schema
