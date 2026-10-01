"""Read-only checks for the authored enemy-role migration (content schema 1)."""
import json
import math
import unreal


def validate_content_milestone(enemies, skills, stages):
    roles = {15101: ('Chaser', 'Sweep'), 15102: ('Shooter', 'AimedProjectile'),
             15103: ('Guardian', 'Sweep'), 15104: ('Crusher', 'ChargeSlam'),
             15105: ('Warden', 'HazardSequence')}
    if all(enemies[eid].get('Role', 'Legacy') == 'Legacy' for eid in roles):
        assert not unreal.EditorAssetLibrary.does_asset_exist('/Game/DataCenter/ContentMilestone/M_PatternBounds'), 'Migrated roles unexpectedly reverted to Legacy'
        return 0  # Legacy assets remain valid until the explicit migration is applied.
    for eid, (role, pattern) in roles.items():
        enemy, skill = enemies[eid], skills[eid]
        assert enemy['Role'] == role and enemy['SkillIdList'] == [eid], eid
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
    unreal.log('PGContent VALIDATION PASS ' + json.dumps(dict(schema=1, roles=5, waves=15, wave_counts=expected)))
    return 1
