#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGData/DataTable/Skill/PGAttackPattern.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGAttackGeometryTest, "PG.Content.AttackGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGAttackGeometryTest::RunTest(const FString&)
{
    const FVector Origin(200,-50,0), Forward(0,1,0);
    TestTrue(TEXT("Circle includes edge"), PGAttackGeometry::Contains(Origin+FVector(100,0,0),Origin,Forward,100));
    TestFalse(TEXT("Circle excludes outside"), PGAttackGeometry::Contains(Origin+FVector(100.1,0,0),Origin,Forward,100));
    TestTrue(TEXT("Sector faces world direction"), PGAttackGeometry::Contains(Origin+FVector(0,90,0),Origin,Forward,100,45));
    TestFalse(TEXT("Sector excludes flank"), PGAttackGeometry::Contains(Origin+FVector(90,0,0),Origin,Forward,100,45));
    TestFalse(TEXT("Sector excludes rear"), PGAttackGeometry::Contains(Origin+FVector(0,-90,0),Origin,Forward,100,80));
    TestTrue(TEXT("Sector includes pivot"), PGAttackGeometry::Contains(Origin,Origin,Forward,100,45));
    TestTrue(TEXT("Line includes end and width edge"),PGAttackGeometry::InLine(Origin+FVector(20,300,0),Origin,Forward,300,20));
    TestFalse(TEXT("Line excludes behind attacker"),PGAttackGeometry::InLine(Origin+FVector(0,-1,0),Origin,Forward,300,20));
    TestFalse(TEXT("Line excludes side"),PGAttackGeometry::InLine(Origin+FVector(21,200,0),Origin,Forward,300,20));
    TestFalse(TEXT("Ring has a safe pocket at its center"), PGAttackGeometry::InRing(Origin + FVector(0,99,0), Origin,100,300));
    TestTrue(TEXT("Ring inner boundary is dangerous"), PGAttackGeometry::InRing(Origin + FVector(100,0,0),Origin,100,300));
    TestTrue(TEXT("Ring outer boundary is dangerous"), PGAttackGeometry::InRing(Origin + FVector(0,300,0),Origin,100,300));
    TestFalse(TEXT("Ring ends at the visible outer edge"), PGAttackGeometry::InRing(Origin + FVector(0,301,0),Origin,100,300));
    TestEqual(TEXT("Fan left lane"), PGAttackGeometry::VolleyAngle(0,3,24), -24.f);
    TestEqual(TEXT("Fan center lane"), PGAttackGeometry::VolleyAngle(1,3,24), 0.f);
    TestEqual(TEXT("Fan right lane"), PGAttackGeometry::VolleyAngle(2,3,24), 24.f);
    FPGSkillDataRow Row;
    Row.TelegraphDuration = 1.f;
    TestTrue(TEXT("Existing timed slam is compatible by default"), Row.IsPatternValid());
    Row.Pattern = EPGAttackPattern::Sweep; Row.TelegraphRadius = 180; Row.SkillRange = 220;
    TestEqual(TEXT("AI must close to the real sweep radius"), Row.GetPatternActivationRange(), 180.f);
    Row.Pattern = EPGAttackPattern::AimedProjectile; Row.SkillRange = 2000; Row.TravelDistance = 1000;
    TestEqual(TEXT("AI cannot shoot beyond projectile travel"), Row.GetPatternActivationRange(), 1000.f);
    Row.TravelSpeed = 0;
    TestFalse(TEXT("Zero travel speed would trap the attack forever"), Row.IsPatternValid());
    Row.TravelSpeed = 1000; Row.Pattern = EPGAttackPattern::HazardSequence; Row.HazardInterval = 0;
    TestFalse(TEXT("Zero interval cannot silently clear the damage timer"), Row.IsPatternValid());
    Row.HazardInterval = .5f; Row.HazardCount = 9;
    TestFalse(TEXT("Hazard sequence stays within the authored budget"), Row.IsPatternValid());
    Row.HazardCount = 3; Row.Pattern = EPGAttackPattern::Thrust; Row.TravelDistance = 400; Row.SkillRange = 800;
    Row.MinimumActivationRange = 200;
    TestEqual(TEXT("Thrust activation agrees with its line length"), Row.GetPatternActivationRange(), 400.f);
    TestFalse(TEXT("Gap-closer is not selected point blank"), Row.IsInActivationRange(199));
    TestTrue(TEXT("Both activation boundaries are inclusive"), Row.IsInActivationRange(200) && Row.IsInActivationRange(400));
    Row.MinimumActivationRange = 400;
    TestFalse(TEXT("An empty activation interval is rejected"), Row.IsPatternValid());
    Row.MinimumActivationRange = 0; Row.Pattern = EPGAttackPattern::RingBurst; Row.TelegraphRadius = 400; Row.InnerSafeRadius = 180;
    TestTrue(TEXT("Ring data is valid"), Row.IsPatternValid());
    Row.InnerSafeRadius = 400;
    TestFalse(TEXT("Ring requires a nonempty damage band"), Row.IsPatternValid());
    Row.Pattern = EPGAttackPattern::AimedProjectile; Row.ProjectileCount = 6;
    TestFalse(TEXT("Fan cannot exceed its performance budget"), Row.IsPatternValid());
    Row.ProjectileCount = 3; Row.ProjectileSpreadHalfAngle = 0;
    TestFalse(TEXT("Multishot cannot stack invisible identical lanes"), Row.IsPatternValid());
    Row.Pattern = EPGAttackPattern::Summon;
    TestFalse(TEXT("Summon requires a registered enemy ID"), Row.IsPatternValid());
    Row.SummonEnemyID = 15404;
    TestTrue(TEXT("Bounded summon data is valid"), Row.IsPatternValid());
    Row.MaxLivingSummons = 0;
    TestFalse(TEXT("Summon cannot omit a population budget"), Row.IsPatternValid());
    Row.MaxLivingSummons = 4; Row.SummonCount = 9;
    TestFalse(TEXT("Summon burst is bounded"), Row.IsPatternValid());
    Row.SummonCount = 2; Row.SummonRadius = -1;
    TestFalse(TEXT("Invalid summon placement radius is rejected"), Row.IsPatternValid());
    return true;
}
#endif
