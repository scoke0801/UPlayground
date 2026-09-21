#pragma once
#include "CoreMinimal.h"
#include "PGData/DataTable/Stage/PGStageDataRow.h"

namespace PGStageValidation
{
inline bool Validate(const FPGStageDataRow& Data, FString& Error)
{
    if (Data.Id <= 0 || Data.MonsterSpawnInfos.IsEmpty() || Data.SpawnBatchSize <= 0 ||
        !FMath::IsFinite(Data.SpawnInterval) || Data.SpawnInterval < 0.f ||
        !FMath::IsFinite(Data.NextStageDelay) || Data.NextStageDelay < 0.f ||
        !FMath::IsFinite(Data.SpawnRadius) || Data.SpawnRadius < 100.f || Data.MaxSpawnRetries < 1)
    {
        Error = TEXT("Invalid stage ID, batch size, retry count, interval, radius or empty spawn list.");
        return false;
    }
    int64 Total = 0;
    for (const FPGMonsterSpawnInfo& Spawn : Data.MonsterSpawnInfos)
    {
        Total += Spawn.SpawnCount;
        if (Spawn.MonsterId <= 0 || Spawn.SpawnCount <= 0 || Total > MAX_int32 ||
            !FMath::IsFinite(Spawn.SpawnDelayTime) || Spawn.SpawnDelayTime < 0.f)
        {
            Error = FString::Printf(TEXT("Invalid spawn entry for monster %d."), Spawn.MonsterId);
            return false;
        }
    }
    for (const FPGStageReward& Reward : Data.RewardPool)
    {
        if (Reward.RewardType == EPGRewardType::None || Reward.RewardId <= 0 ||
            !FMath::IsFinite(Reward.Weight) || Reward.Weight <= 0.f)
        {
            Error = FString::Printf(TEXT("Invalid reward %d."), Reward.RewardId);
            return false;
        }
    }
    Error.Reset();
    return true;
}
}