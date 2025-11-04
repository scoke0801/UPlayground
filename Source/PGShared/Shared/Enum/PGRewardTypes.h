#pragma once

#include "CoreMinimal.h"
#include "PGRewardTypes.generated.h"

UENUM(BlueprintType)
enum class EPGRewardType : uint8
{
    None = 0,

    Item,
    Stat,
    Skill
};