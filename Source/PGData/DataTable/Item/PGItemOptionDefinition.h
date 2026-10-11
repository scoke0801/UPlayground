#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"
#include "PGItemOptionDefinition.generated.h"

// Additive integer units, identical to Options/GAS (CriticalRate is 0.01%).
USTRUCT(BlueprintType)
struct PGDATA_API FPGItemOptionDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName Id;
    UPROPERTY(EditAnywhere) FText DisplayName;
    UPROPERTY(EditAnywhere) EPGStatType Stat = EPGStatType::None;
    UPROPERTY(EditAnywhere) bool bEnabled = true;
    UPROPERTY(EditAnywhere) TArray<EPGEquipmentSlot> AllowedSlots;
    UPROPERTY(EditAnywhere) FName ExclusiveGroup;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float Weight = 1.f;
    UPROPERTY(EditAnywhere) bool bAllowGreater = true;
    // Endpoints at MinPower/MaxPower; linear interpolation rounds to the nearest integer.
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) int32 MinAtLowPower = 0;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) int32 MaxAtLowPower = 0;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) int32 MinAtHighPower = 0;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) int32 MaxAtHighPower = 0;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="100000")) int32 Cap = 100000;
};
