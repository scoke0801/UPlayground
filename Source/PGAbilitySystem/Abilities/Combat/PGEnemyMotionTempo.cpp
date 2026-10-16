#include "PGEnemyAbilityAttack.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UPGEnemyAbilityAttack::SetMotionSegment(float StartFraction, float EndFraction, double StartAt, double EndAt)
{
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    auto* Anim = Enemy ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
    auto* Montage = EliteData.ElitePresentationMontage.Get();
    if (!IsActive() || !Anim || !Montage) return;
    const float Duration = FMath::Max(.001f, float(EndAt - StartAt));
    const float Progress = FMath::Clamp(float(GetWorld()->GetTimeSeconds() - StartAt) / Duration, 0.f, 1.f);
    // Correct a late timer at the phase boundary; native montage playback advances every rendered frame.
    Anim->Montage_SetPosition(Montage, Montage->GetPlayLength() * FMath::Lerp(StartFraction, EndFraction, Progress));
    Anim->Montage_SetPlayRate(Montage, Montage->GetPlayLength() * (EndFraction - StartFraction) / Duration);
    // A zero rate alone accumulates montage substep time and skips the swing on resume.
    if (FMath::IsNearlyEqual(StartFraction, EndFraction)) Anim->Montage_Pause(Montage);
    else Anim->Montage_Resume(Montage);
}

void UPGEnemyAbilityAttack::BeginMotionHold()
{
    if (!IsActive() || !EliteData.MotionTempo.bEnabled || bStriking) return;
    const double Now = GetWorld()->GetTimeSeconds();
    const double SwingAt = PatternStartedAt + EliteData.TelegraphDuration - EliteData.MotionTempo.SwingSeconds;
    const float Remaining = float(SwingAt - Now);
    if (Remaining <= UE_SMALL_NUMBER) { BeginMotionSwing(); return; }
    SetMotionSegment(EliteData.MotionTempo.SwingStartFraction, EliteData.MotionTempo.SwingStartFraction, Now, SwingAt);
    GetWorld()->GetTimerManager().SetTimer(MotionTimer, this, &ThisClass::BeginMotionSwing, Remaining, false);
}

void UPGEnemyAbilityAttack::BeginMotionSwing()
{
    if (!IsActive() || !EliteData.MotionTempo.bEnabled || bStriking) return;
    const double ImpactAt = PatternStartedAt + EliteData.TelegraphDuration;
    SetMotionSegment(EliteData.MotionTempo.SwingStartFraction, EliteData.ImpactMontageFraction,
        ImpactAt - EliteData.MotionTempo.SwingSeconds, ImpactAt);
}

void UPGEnemyAbilityAttack::BeginMotionSettle()
{
    auto* Enemy = GetEnemyCharacterFromActorInfo();
    if (!IsActive() || !Enemy || !Enemy->bPatternRecovering || !EliteData.MotionTempo.bEnabled) return;
    SetMotionSegment(EliteData.MotionTempo.SettleStartFraction, 1.f,
        RecoveryStartedAt + EliteData.MotionTempo.FollowThroughSeconds, RecoveryStartedAt + EliteData.RecoveryDuration);
}
