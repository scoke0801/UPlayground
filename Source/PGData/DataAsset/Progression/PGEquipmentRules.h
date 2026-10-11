#pragma once

#include "CoreMinimal.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"
#include "PGEquipmentRules.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGEquipmentRarityRule
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) EPGItemRarity Rarity = EPGItemRarity::Common;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="4")) int32 OptionCount = 0;
    UPROPERTY(EditAnywhere) bool bFixedOptions = false;
    UPROPERTY(EditAnywhere) bool bAllowAncestral = false;
    UPROPERTY(EditAnywhere) bool bAllowTemper = false;
    UPROPERTY(EditAnywhere) bool bAllowImprint = false;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="5")) int32 MaxMasterwork = 0;
    UPROPERTY(EditAnywhere) bool bDropEnabled = false;
};

struct FPGItemDataRow;
class UPGProgressionData;

namespace PGEquipmentRules
{
    PGDATA_API bool FitsSlot(const FPGItemDataRow& Def, EPGEquipmentSlot Slot);
    PGDATA_API int32 BagCount(const TArray<FPGItemInstance>& Items, const TMap<EPGEquipmentSlot, FGuid>& Equipment);
    PGDATA_API bool PlanEquip(const UPGProgressionData& Catalog, const TArray<FPGItemInstance>& Items,
        const TMap<EPGEquipmentSlot, FGuid>& Equipment, FGuid Guid, EPGEquipmentSlot Target,
        TMap<EPGEquipmentSlot, FGuid>& Out, FString& Error);
    PGDATA_API TArray<FPGEquipmentRarityRule> DefaultRarities();
    PGDATA_API bool ValidateRarities(const TArray<FPGEquipmentRarityRule>& Rules, FString& Error);
    PGDATA_API bool ValidateInstance(const FPGItemInstance& Item);
}
