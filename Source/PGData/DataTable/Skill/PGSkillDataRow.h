#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGAttackPattern.h"
#include "PGData/Combat/PGEnemyAttackProfile.h"
#include "PGSkillDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGSkillDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 SkillID = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Desc;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EPGSkillType SkillType = {};
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSoftObjectPath MontagePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSoftObjectPath SkillIconPath;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int32> ChainSkillIdList;

    // Time allowed after the current attack's montage for the next chain input.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Combo", meta=(ClampMin="0", ClampMax="2"))
    float ComboResetSeconds = .4f;

    // Multipliers on the existing montage and confirmed melee hit; ranged notifies retain their own damage.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Attack", meta=(ClampMin="0.5", ClampMax="2"))
    float PlayerAttackPlayRate = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Attack")
    TSoftObjectPtr<class UPGPlayerSkillProfile> PlayerProfile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Attack", meta=(ClampMin="0.1", ClampMax="5"))
    float PlayerMeleeDamageMultiplier = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Attack")
    bool bPlayerHeavyImpact = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SkillCoolTime = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 InitialPriority = 1;

	/** 스킬 사용 가능 범위 (0이면 범위 제한 없음) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float SkillRange = 0.f;

    // AI selection only; the committed shape remains fixed after aim lock.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0"))
    float MinimumActivationRange = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0"))
    float SelectionWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="1", ClampMax="3"))
    int32 AttackPressureCost = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cancel", meta=(ClampMin="0", ClampMax="1"))
    float AttackCancelRemainingFraction = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cancel", meta=(ClampMin="0", ClampMax="1"))
    float DodgeCancelRemainingFraction = 0.5f;

    // A positive duration opts into a timed attack; zero keeps the original montage path.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern")
    EPGAttackPattern Pattern = EPGAttackPattern::LegacySlam;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern")
    TSoftObjectPtr<UPGEnemyAttackProfile> EnemyProfile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="0.01"))
    float EnemyDamageMultiplier = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1", ClampMax="180"))
    float HalfAngleDegrees = 65.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="0"))
    float AimTrackingSeconds = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1"))
    float TravelDistance = 650.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1"))
    float TravelSpeed = 1100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="0.05"))
    float LandingTelegraphSeconds = .25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1"))
    float LineHalfWidth = 70.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1"))
    float InnerSafeRadius = 160.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1", ClampMax="5"))
    int32 ProjectileCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="0", ClampMax="60"))
    float ProjectileSpreadHalfAngle = 22.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1", ClampMax="8"))
    int32 HazardCount = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="0.1"))
    float HazardInterval = .55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="0"))
    float HazardSpacing = 240.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1", ClampMax="2"))
    int32 MinimumBossPhase = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern")
    TSoftClassPtr<class AActor> ProjectileClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern")
    TSoftObjectPtr<class USoundBase> AttackSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="0"))
    float TelegraphDuration = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="0"))
    float RecoveryDuration = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="0", ClampMax="2"))
    float RecoveryDamageBonus = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="1"))
    float TelegraphRadius = 280.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph")
    TSoftObjectPtr<class UMaterialInterface> TelegraphMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph")
    TSoftObjectPtr<class UNiagaraSystem> SlamVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph")
    TSoftObjectPtr<class UAnimMontage> ElitePresentationMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="0", ClampMax="1"))
    float WindupMontageFraction = .25f;

    // Opt-in: play anticipation up to the authored impact pose, then follow-through
    // over the existing recovery window. The pattern timer remains damage authority.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph")
    bool bSyncMontageToPattern = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="0.01", ClampMax="0.99"))
    float ImpactMontageFraction = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="0"))
    float ImpactVFXScale = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph")
    bool bHeavyImpactFeedback = true;

    /** Reject malformed imported data before committing cooldowns or starting timers. */
    bool IsPatternValid() const
    {
        if (!FMath::IsFinite(EnemyDamageMultiplier) || EnemyDamageMultiplier <= 0.f ||
            (!EnemyProfile.IsNull() && (!EnemyProfile.LoadSynchronous() || !EnemyProfile.Get()->IsValid()))) return false;
        if (!FMath::IsFinite(SkillRange) || SkillRange < 0.f ||
            !FMath::IsFinite(MinimumActivationRange) || MinimumActivationRange < 0.f ||
            MinimumActivationRange >= GetPatternActivationRange() ||
            !FMath::IsFinite(SelectionWeight) || SelectionWeight < 0.f ||
            AttackPressureCost < 1 || AttackPressureCost > 3) return false;
        if (!FMath::IsFinite(ImpactVFXScale) || ImpactVFXScale < 0.f) return false;
        if (bSyncMontageToPattern && (!FMath::IsFinite(WindupMontageFraction) ||
            !FMath::IsFinite(ImpactMontageFraction) || WindupMontageFraction < 0.f ||
            ImpactMontageFraction <= WindupMontageFraction || ImpactMontageFraction >= 1.f)) return false;
        if (!FMath::IsFinite(TelegraphDuration) || TelegraphDuration <= 0.f ||
            !FMath::IsFinite(TelegraphRadius) || TelegraphRadius <= 0.f ||
            !FMath::IsFinite(RecoveryDuration) || RecoveryDuration < 0.f ||
            !FMath::IsFinite(RecoveryDamageBonus) || RecoveryDamageBonus < 0.f ||
            !FMath::IsFinite(AimTrackingSeconds) || AimTrackingSeconds < 0.f ||
            !FMath::IsFinite(HalfAngleDegrees) || HalfAngleDegrees <= 0.f || HalfAngleDegrees > 180.f) return false;
        if (Pattern == EPGAttackPattern::ChargeSlam || Pattern == EPGAttackPattern::AimedProjectile)
            if (!FMath::IsFinite(TravelDistance) || TravelDistance <= 0.f ||
                !FMath::IsFinite(TravelSpeed) || TravelSpeed <= 0.f ||
                !FMath::IsFinite(LineHalfWidth) || LineHalfWidth <= 0.f ||
                !FMath::IsFinite(LandingTelegraphSeconds) || LandingTelegraphSeconds < .05f) return false;
        if (Pattern == EPGAttackPattern::HazardSequence)
            if (HazardCount < 1 || HazardCount > 8 || !FMath::IsFinite(HazardInterval) || HazardInterval < .1f ||
                !FMath::IsFinite(HazardSpacing) || HazardSpacing < 0.f) return false;
        if (Pattern == EPGAttackPattern::Thrust &&
            (!FMath::IsFinite(TravelDistance) || TravelDistance <= 0.f ||
             !FMath::IsFinite(LineHalfWidth) || LineHalfWidth <= 0.f)) return false;
        if (Pattern == EPGAttackPattern::RingBurst &&
            (!FMath::IsFinite(InnerSafeRadius) || InnerSafeRadius <= 0.f || InnerSafeRadius >= TelegraphRadius)) return false;
        if (Pattern == EPGAttackPattern::AimedProjectile &&
            (ProjectileCount < 1 || ProjectileCount > 5 || !FMath::IsFinite(ProjectileSpreadHalfAngle) ||
             ProjectileSpreadHalfAngle < 0.f || ProjectileSpreadHalfAngle > 60.f ||
             (ProjectileCount > 1 && ProjectileSpreadHalfAngle <= 0.f))) return false;
        return Pattern >= EPGAttackPattern::LegacySlam && Pattern <= EPGAttackPattern::RingBurst;
    }

    float GetPatternActivationRange() const
    {
        float Range = SkillRange > 0.f ? SkillRange : TNumericLimits<float>::Max();
        if (TelegraphDuration > 0.f && (Pattern == EPGAttackPattern::Sweep || Pattern == EPGAttackPattern::RingBurst)) Range = FMath::Min(Range, TelegraphRadius);
        if (TelegraphDuration > 0.f && (Pattern == EPGAttackPattern::AimedProjectile || Pattern == EPGAttackPattern::Thrust)) Range = FMath::Min(Range, TravelDistance);
        return Range;
    }

    bool IsInActivationRange(float Distance) const
    {
        return FMath::IsFinite(Distance) && Distance >= MinimumActivationRange && Distance <= GetPatternActivationRange();
    }
	
};
