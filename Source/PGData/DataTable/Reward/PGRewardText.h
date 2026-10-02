#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Enum/PGRewardTypes.h"
class UPGCombatTuningData;
struct FPGRewardStatDataRow;

namespace PGRewardText
{
    PGDATA_API FString PerkName(EPGCombatPerk Perk);
    PGDATA_API FString Effect(EPGCombatPerk Perk, int32 Value, const UPGCombatTuningData& Tuning);
    PGDATA_API FString Requirements(const FPGRewardStatDataRow& Reward);
}
