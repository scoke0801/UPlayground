#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PGHumanoidLocomotionTools.generated.h"

/** Narrow editor migration: ground locomotion only; airborne/IK/montage graph stays connected. */
UCLASS()
class PGBLUEPRINTUTIL_API UPGHumanoidLocomotionTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="PG|Editor")
    static bool RemoveRetargetedTurnYaw(class UAnimSequence* Animation, const TArray<float>& RotationProgress, float Angle);

    UFUNCTION(BlueprintCallable, Category="PG|Editor")
    static bool ConfigurePlayerTurnLocomotion(class UAnimBlueprint* Blueprint, class UAnimSequence* DefaultTurn);

    UFUNCTION(BlueprintCallable, Category="PG|Editor")
    static bool ConfigurePlayerGroundLocomotion(class UAnimBlueprint* Blueprint, class UBlendSpace* BlendSpace);

    UFUNCTION(BlueprintCallable, Category="PG|Editor")
    static void RebuildBlendSpace(class UBlendSpace* BlendSpace);

    UFUNCTION(BlueprintCallable, Category="PG|Editor")
    static int32 GetBlendSampleCount(class UBlendSpace* BlendSpace, FVector Input);
};
