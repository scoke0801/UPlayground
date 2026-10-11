#include "PGEnemyAbilityAttack.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

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
