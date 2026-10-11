#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"
#include "PGSalvageRules.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGSalvageYield
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) EPGItemRarity Rarity = EPGItemRarity::Common;
    UPROPERTY(EditAnywhere) TMap<FName, int32> Materials;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1")) int32 PowerStep = 25;
    UPROPERTY(EditAnywhere) TMap<FName, int32> PerPowerStep;
};

USTRUCT(BlueprintType)
struct PGDATA_API FPGSalvageRules
{
    GENERATED_BODY()
    FPGSalvageRules();
    UPROPERTY(EditAnywhere) TMap<FName, FText> MaterialNames;
    UPROPERTY(EditAnywhere) TArray<FPGSalvageYield> Yields;
    bool IsValid() const;
    bool GetYield(const FPGItemInstance& Item, TMap<FName, int64>& Out) const;
};
