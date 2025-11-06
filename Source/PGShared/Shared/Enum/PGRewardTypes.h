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

UENUM(BlueprintType)
enum class EPGRewardGrade : uint8
{
    Normal = 0,
    Magic,
    Rare
};

