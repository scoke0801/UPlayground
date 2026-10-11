#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"
#include "PGCraftingRules.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGTemperRecipe
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName Id;
    UPROPERTY(EditAnywhere) FText DisplayName;
    UPROPERTY(EditAnywhere) TSet<EPGEquipmentSlot> Slots;
    UPROPERTY(EditAnywhere) EPGStatType Stat = EPGStatType::Attack;
    UPROPERTY(EditAnywhere) int32 BaseValue = 2;
    UPROPERTY(EditAnywhere) int32 PowerStep = 10;
    UPROPERTY(EditAnywhere) int32 ValuePerStep = 1;
    UPROPERTY(EditAnywhere) int32 MaxValue = 100;
    UPROPERTY(EditAnywhere) bool bAllowAffixDuplicate = true;
    UPROPERTY(EditAnywhere) TMap<FName, int64> Cost;
};

USTRUCT(BlueprintType)
struct PGDATA_API FPGMasterworkStep
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) EPGEquipmentSlot Slot = EPGEquipmentSlot::Weapon;
    UPROPERTY(EditAnywhere) EPGItemRarity Rarity = EPGItemRarity::Rare;
    UPROPERTY(EditAnywhere) int32 Level = 1;
    // Each step adds this percentage of the original saved base performance.
    UPROPERTY(EditAnywhere) int32 BasePercent = 5;
    UPROPERTY(EditAnywhere) TMap<FName, int64> Cost;
};

class UPGProgressionData;
namespace PGCraftingRules
{
    PGDATA_API TArray<FPGTemperRecipe> DefaultTempers();
    PGDATA_API TArray<FPGMasterworkStep> DefaultMasterworks();
    PGDATA_API bool Validate(const UPGProgressionData& Catalog, FString& Reason);
    // NAME_None selects the next masterwork step; a recipe ID selects tempering.
    PGDATA_API bool Preview(const UPGProgressionData& Catalog, const FPGItemInstance& Item, FName Recipe,
        FPGItemInstance& Result, TMap<FName, int64>& Cost, FString& Reason);
}
