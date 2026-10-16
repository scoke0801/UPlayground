#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "Animation/AnimMontage.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGEnemyMotionTempoTest, "PG.Content.EnemyMotionTempo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGEnemyMotionTempoTest::RunTest(const FString&)
{
    FPGSkillDataRow Row;
    Row.Pattern = EPGAttackPattern::Sweep;
    Row.TelegraphDuration = 1.f;
    Row.RecoveryDuration = 1.2f;
    Row.WindupMontageFraction = 0.f;
    Row.ImpactMontageFraction = .175f;
    Row.bSyncMontageToPattern = true;
    Row.ElitePresentationMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Test/Motion.Motion")));
    Row.MotionTempo.bEnabled = true;
    Row.MotionTempo.SwingStartFraction = .06f;
    Row.MotionTempo.SwingSeconds = .16f;
    Row.MotionTempo.SettleStartFraction = .55f;
    Row.MotionTempo.FollowThroughSeconds = .42f;
    Row.AimTurnRate = 180.f;
    TestTrue(TEXT("Authored greatsword phases fit unchanged damage/recovery windows"), Row.IsPatternValid());
    Row.MotionTempo.AnticipationPoseSeconds = 0.f;
    TestFalse(TEXT("Ready-pose transition must have a positive duration"), Row.IsPatternValid());
    Row.MotionTempo.AnticipationPoseSeconds = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite ready-pose time is rejected"), Row.IsPatternValid());
    Row.MotionTempo.AnticipationPoseSeconds = .18f;
    Row.MotionTempo.SwingSeconds = 1.f;
    TestFalse(TEXT("Swing must leave readable anticipation"), Row.IsPatternValid());
    Row.MotionTempo.SwingSeconds = .16f;
    Row.MotionTempo.SwingStartFraction = .18f;
    TestFalse(TEXT("Windup cannot run backward from contact"), Row.IsPatternValid());
    Row.MotionTempo.SwingStartFraction = .06f;
    Row.MotionTempo.SettleStartFraction = .1f;
    TestFalse(TEXT("Follow-through cannot rewind contact"), Row.IsPatternValid());
    Row.MotionTempo.SettleStartFraction = .55f;
    Row.MotionTempo.FollowThroughSeconds = Row.RecoveryDuration;
    TestFalse(TEXT("Follow-through leaves time for stance recovery"), Row.IsPatternValid());
    Row.MotionTempo.FollowThroughSeconds = .42f;
    Row.AimTurnRate = -1.f;
    TestFalse(TEXT("Negative tracking rate is rejected"), Row.IsPatternValid());
    Row.AimTurnRate = 180.f;
    Row.MotionTempo.SwingSeconds = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite imported timing cannot reach montage playback"), Row.IsPatternValid());
    Row.MotionTempo.SwingSeconds = .16f;
    Row.bSyncMontageToPattern = false;
    TestFalse(TEXT("Tempo cannot opt into an unsynchronized montage path"), Row.IsPatternValid());
    Row.bSyncMontageToPattern = true;
    for (const auto Pattern : {EPGAttackPattern::ChargeSlam, EPGAttackPattern::HazardSequence, EPGAttackPattern::AimedProjectile})
    {
        Row.Pattern = Pattern;
        TestFalse(TEXT("Multi-stage patterns cannot accidentally opt into single-contact tempo"), Row.IsPatternValid());
    }
    Row.Pattern = EPGAttackPattern::Sweep;
    Row.MotionTempo.bEnabled = false;
    Row.MotionTempo.SwingSeconds = -1.f;
    TestTrue(TEXT("Existing disabled data retains its original path"), Row.IsPatternValid());
    return true;
}
#endif
