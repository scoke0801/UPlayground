"""Read-back checks for step 3; fixed schema, no inferred expected content counts."""
import math
import unreal

def validate_build_keystones(rewards, stages, tuning):
    assert len([i for i in rewards if 15000<=i<15100])==21
    expected=[(15018,'BleedRecast','Bleed',['BleedBurst'],[],0),
              (15019,'ShockFracture','Shockwave',[],['ShockRadius','ShockEcho','ShockExecute'],1),
              (15020,'FrenzyAfterimage','Frenzy',[],['FrenzyDuration','FrenzyLeech','FrenzyGuard'],2)]
    for rid,perk,root,all_of,any_of,panel in expected:
        row=rewards[rid]
        assert row['Perk']==perk and row['PerkPercent']==1 and row['Amount']==0
        assert row['RequiredPerk']==root and row['RequiredPerks']==all_of and row['RequiredAnyPerks']==any_of
        assert row['bKeystone'] and row['MaxSelections']==1
        assert row['IconPanel']==panel and unreal.load_asset(row['Icon'])
    for row in rewards.values():
        if not 15000<=row['StatId']<15100: continue
        assert row['MaxSelections']>0 and row['DisplayName'] and row['PlaystyleDescription']
    for stage in stages:
        cores={r['RewardId'] for r in stage['RewardPool'] if 15018<=r['RewardId']<=15020}
        assert cores==({15018,15019,15020} if stage['Id'] in (4,5) else set())
        assert stage['bReserveKeystoneChoice']==(stage['Id'] in (4,5))
    for key in ('bleed_refund_fraction','shock_defense_reduction'):
        value=tuning.get_editor_property(key)
        assert math.isfinite(value) and 0<value<=1,key
    for key in ('shock_fracture_hits','shock_stack_seconds','shock_weakness_seconds','afterimage_radius','afterimage_attack_multiplier','build_proc_display_seconds'):
        value=tuning.get_editor_property(key)
        assert math.isfinite(value) and value>0,key
    assert tuning.get_editor_property('afterimage_vfx')
    unreal.log('PGBuildKeystones VALIDATION PASS schema=3 rewards=21')
    return 3
