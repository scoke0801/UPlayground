#pragma once

#include "CoreMinimal.h"
#include "PGRewardTypes.generated.h"

// 메시지 타입 enum (uint8 기반)
UENUM(BlueprintType)
enum class EPGRewardType : uint8
{
    None = 0,

    Item,
    Stat,
    Skill
};