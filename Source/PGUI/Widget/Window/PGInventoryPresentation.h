#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"

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
    PGUI_API FText StatName(EPGStatType Stat);
    PGUI_API FText StatValue(EPGStatType Stat, int32 Value, bool bDelta = false);
    PGUI_API FText RarityName(EPGItemRarity Rarity);
    PGUI_API FText SlotName(EPGEquipmentSlot Slot);
    PGUI_API TArray<FPGInventoryStatComparison> Compare(const FPGItemInstance& Selected, const FPGItemInstance* Current);
}
