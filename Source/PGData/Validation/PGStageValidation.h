#pragma once
#include "CoreMinimal.h"
#include "PGData/DataTable/Stage/PGStageDataRow.h"

namespace PGStageValidation
{
inline bool Validate(const FPGStageDataRow& Data, FString& Error)
{
    if (Data.RewardSelections < 1 || Data.RewardSelections > 3 || (!Data.bManualReady && Data.RewardSelections > 1) || Data.Id <= 0 || (Data.Waves.IsEmpty() && Data.MonsterSpawnInfos.IsEmpty()) || Data.SpawnBatchSize <= 0 ||
        !FMath::IsFinite(Data.SpawnInterval) || Data.SpawnInterval < 0.f ||
        !FMath::IsFinite(Data.BuildDuration) || Data.BuildDuration < 0.f ||
        !FMath::IsFinite(Data.SpawnRadius) || Data.SpawnRadius < 100.f || Data.MaxSpawnRetries < 1)
    {
        Error = TEXT("Invalid stage ID, batch size, retry count, interval, radius or empty spawn list.");
        return false;
    }
    int64 Total = 0;
    const auto ValidateSpawns = [&](const TArray<FPGMonsterSpawnInfo>& Spawns)
    {
        if (Spawns.IsEmpty()) { Error = TEXT("Wave spawn list is empty."); return false; }
        for (const FPGMonsterSpawnInfo& Spawn : Spawns)
        {
            Total += Spawn.SpawnCount;
            if (Spawn.MonsterId <= 0 || Spawn.SpawnCount <= 0 || Total > MAX_int32 ||
                !FMath::IsFinite(Spawn.SpawnDelayTime) || Spawn.SpawnDelayTime < 0.f)
            {
                Error = FString::Printf(TEXT("Invalid spawn entry for monster %d."), Spawn.MonsterId);
                return false;
            }
        }
        return true;
    };
    if (Data.Waves.IsEmpty())
    {
        if (!ValidateSpawns(Data.MonsterSpawnInfos)) return false;
    }
    else for (const FPGStageWave& Wave : Data.Waves)
    {
        if (!FMath::IsFinite(Wave.StartDelay) || Wave.StartDelay < 0.f)
        { Error = TEXT("Invalid wave start delay."); return false; }
        if (!ValidateSpawns(Wave.MonsterSpawnInfos)) return false;
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
