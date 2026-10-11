#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"
#include "PGShared/Shared/Enum/PGRewardTypes.h"
#include "PGLegendaryRules.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGLegendaryEffect
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName Id;
    UPROPERTY(EditAnywhere) FText DisplayName;
    UPROPERTY(EditAnywhere) FText Description;
    UPROPERTY(EditAnywhere) EPGCombatPerk Perk = EPGCombatPerk::None;
    UPROPERTY(EditAnywhere) int32 Percent = 10;
    // Missing slots cannot receive this effect; all default multipliers are one.
    UPROPERTY(EditAnywhere) TMap<EPGEquipmentSlot, float> SlotMultipliers;
    UPROPERTY(EditAnywhere) TMap<FName, int64> Cost;
};

class UPGProgressionData;
namespace PGLegendaryRules
{
    PGDATA_API TArray<FPGLegendaryEffect> Defaults();
    PGDATA_API bool Validate(const UPGProgressionData& Catalog, FString& Error);
    PGDATA_API int32 Strength(const FPGLegendaryEffect& Effect, EPGEquipmentSlot Slot);
    PGDATA_API bool ValidateItem(const UPGProgressionData& Catalog, const FPGItemInstance& Item);
    PGDATA_API TMap<FName, FGuid> Active(const UPGProgressionData& Catalog, const TArray<FPGItemInstance>& Items, const TMap<EPGEquipmentSlot, FGuid>& Equipment);
    PGDATA_API TMap<EPGCombatPerk, int32> Perks(const UPGProgressionData& Catalog, const TArray<FPGItemInstance>& Items, const TMap<EPGEquipmentSlot, FGuid>& Equipment);
}
