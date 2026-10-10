#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGPlayerLocomotionData.generated.h"

class UAnimSequence;

USTRUCT(BlueprintType)
struct PGDATA_API FPGPlayerTurnMotion
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Animation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float Angle = 90.f;
    // Uniform samples of the source root yaw / authored angle; includes anticipation/overshoot.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<float> RotationProgress;
};

/** Authored turn poses and their matching capsule rotation, independent of combat montages. */
UCLASS(BlueprintType)
class PGDATA_API UPGPlayerLocomotionData : public UDataAsset
{
    GENERATED_BODY()
public:
    // Keep the last attack facing briefly so eight-way combat footwork can continue between attacks.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", Units="s")) float CombatStrafeSeconds = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FPGPlayerTurnMotion> Turns;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="180")) float StartTurnAngle = 60.f;
    // Stationary foot plants are unsuitable for a running reversal.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAllowMovingTurns = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="45", ClampMax="180")) float MovingPivotAngle = 125.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float StationarySpeed = 80.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.1")) float TurnPlayRate = 1.3f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.1", Units="s")) float MaxTurnSeconds = .4f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="0.95")) float MovementResumeFraction = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01")) float BlendInSeconds = .07f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01")) float BlendOutSeconds = .12f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="180")) float RetargetCancelAngle = 20.f;
};
