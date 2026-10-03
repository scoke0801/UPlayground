#pragma once
#include "CoreMinimal.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"

namespace PGRewardPresentation
{
    PGUI_API FText BuildContext(const FPGRewardStatDataRow& Reward, TFunctionRef<int32(EPGCombatPerk)> GetPerk);
    PGUI_API bool IsValidChoice(int32 Index, int32 Count);
}
