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

UENUM(BlueprintType)
enum class EPGCombatPerk : uint8
{
    None,
    LifeSteal,
    Execution,
    Counter,
    Bleed,
    BleedPotency,
    BleedSpread,
    BleedBurst,
    Shockwave,
    ShockRadius,
    ShockEcho,
    ShockExecute,
    Frenzy,
    FrenzyDuration,
    FrenzyLeech,
    FrenzyGuard,
    Cooldown,
    BleedRecast,
    ShockFracture,
    FrenzyAfterimage,
    Max UMETA(Hidden)
};

// Secondary damage carries its origin, but never re-enters direct-hit reactions.
enum class EPGDamageCause : uint8
{
    External, BleedTick, BleedBurst, Shockwave, ShockEcho, FrenzyAfterimage
};

