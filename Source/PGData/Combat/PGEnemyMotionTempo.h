#pragma once

#include "CoreMinimal.h"
#include "PGEnemyMotionTempo.generated.h"

/** Preserve readable anticipation without stretching the weapon swing across the whole warning. */
USTRUCT(BlueprintType)
struct PGDATA_API FPGEnemyMotionTempo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float SwingStartFraction = .1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.05")) float SwingSeconds = .18f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float SettleStartFraction = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.05")) float FollowThroughSeconds = .3f;

    bool IsValid(float Start, float Impact, float Windup, float Recovery) const
    {
        return !bEnabled || (FMath::IsFinite(SwingStartFraction) && FMath::IsFinite(SettleStartFraction) &&
            FMath::IsFinite(SwingSeconds) && FMath::IsFinite(FollowThroughSeconds) &&
            Start < SwingStartFraction && SwingStartFraction < Impact && Impact < SettleStartFraction && SettleStartFraction < 1.f &&
            SwingSeconds >= .05f && SwingSeconds <= Windup - .05f &&
            FollowThroughSeconds >= .05f && FollowThroughSeconds <= Recovery - .05f);
    }
};
