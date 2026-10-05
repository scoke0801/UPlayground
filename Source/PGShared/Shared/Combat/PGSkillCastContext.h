#pragma once

#include "CoreMinimal.h"
#include "PGSkillCastContext.generated.h"

USTRUCT(BlueprintType)
struct PGSHARED_API FPGHitProcPolicy
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBleed = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBleedBurst = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShock = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFrenzy = true;
};

// One allocation per committed cast. Never reuse counters across ability activations.
// Weak identities also distinguish a destroyed actor from a new object at the same address.
struct PGSHARED_API FPGSkillCastContext
{
    FGuid CastId = FGuid::NewGuid();
    int32 SkillID = 0;
    float Attack = 0.f;
    TWeakObjectPtr<AActor> Caster;
    bool bRefundEligible = false;
    bool bRefundUsed = false;
    bool bShockUsed = false;
    int32 FrenzyGranted = 0;
    int32 FrenzyCap = 3;
    TSet<TWeakObjectPtr<AActor>> BurstTargets;
    TMap<int32, TSet<TWeakObjectPtr<AActor>>> HitTargets;
    TSet<int32> FeedbackPhases;
    int32 SpatialQueries = 0;
    TWeakPtr<struct FPGSkillObservation> Observation;
};
