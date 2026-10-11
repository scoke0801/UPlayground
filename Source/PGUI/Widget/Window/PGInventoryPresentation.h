#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"

class UPGProgressionData;
enum class EPGInventorySort : uint8 { Recent, Slot, Rarity, Power };
struct FPGInventoryFilter
{
    int32 Slot = -1, Rarity = -1, Quality = -1;
    // 0 = all, 1 = favorite, 2 = junk.
    int32 Mark = 0;
};

struct FPGInventoryStatComparison
{
    EPGStatType Stat = EPGStatType::None;
    int32 Current = 0;
    int32 Selected = 0;
    int32 Delta() const { return Selected - Current; }
};

/** Display-only helpers. Equipment authority remains in the profile subsystem. */
namespace PGInventoryPresentation
{
    PGUI_API TArray<FGuid> BagItems(const TArray<FPGItemInstance>& Items, const TMap<EPGEquipmentSlot, FGuid>& Equipment,
        const UPGProgressionData& Catalog, const FPGInventoryFilter& Filter, EPGInventorySort Sort);
    PGUI_API bool PreviewEquip(const TArray<FPGItemInstance>& Items, const TMap<EPGEquipmentSlot, FGuid>& Equipment,
        const UPGProgressionData& Catalog, FGuid Guid, EPGEquipmentSlot Target,
        TArray<FPGInventoryStatComparison>& Rows, FString& Reason);
    PGUI_API FText StatName(EPGStatType Stat);
    PGUI_API FText StatValue(EPGStatType Stat, float Value, bool bDelta = false);
    PGUI_API FText RarityName(EPGItemRarity Rarity);
    PGUI_API FText SlotName(EPGEquipmentSlot Slot);
    PGUI_API TArray<FPGInventoryStatComparison> Compare(const FPGItemInstance& Selected, const FPGItemInstance* Current);
}
