#pragma once
#include "CoreMinimal.h"
#include "PGInventoryTypes.h"
#include "PGLevelState.generated.h"

USTRUCT(BlueprintType)
struct PGSHARED_API FPGLevelState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 Level = 1;
    UPROPERTY(SaveGame, BlueprintReadOnly) int64 Experience = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 EarnedPoints = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 UnspentPoints = 0;
    UPROPERTY(SaveGame) TMap<EPGStatType, int32> Investments;
};
