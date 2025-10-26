#pragma once

#include "CoreMinimal.h"
#include "PGMessageTypes.generated.h"

// 메시지 타입 enum (uint8 기반)
UENUM(BlueprintType)
enum class EPGMessageType : uint8
{
    None = 0,
    PlayerInit,
    PlayerDeath,
    PlayerLevelUp,
    ItemPickup,
    EnemyDeath,
    GameStart,
    GameEnd,
    UIUpdate,
    Max UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EPGPlayerMessageType : uint8
{
    None = 0,

    UseSkill,
    LevelUp,

    ExpUpdate,
    
    StatUpdate,

    Spawned,
    
    Max UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EPGSharedMessageType : uint8
{
    None = 0,

    OnHit,
    OnDied,
    OnSpawned,
    
    Max UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EPGUIMessageType : uint8
{
    None = 0,

    ClickSkillButton,
    StageChanged,
    
    Max UMETA(Hidden)
};