#pragma once
#include "PGData/DataTable/Stage/PGStageDataRow.h"

namespace PGRewardSelection
{
    inline TArray<FPGStageReward> Draw(TArray<FPGStageReward> Pool, bool bReserveKeystone,
        TFunctionRef<bool(int32)> IsKeystone, FRandomStream& Random)
    {
        TArray<FPGStageReward> Result;
        Pool.Sort([](const auto& A, const auto& B) { return A.RewardId < B.RewardId; });
        TArray<FPGStageReward> Cores;
        for (const auto& Entry : Pool) if (IsKeystone(Entry.RewardId)) Cores.Add(Entry);
        if (bReserveKeystone && !Cores.IsEmpty()) Result.Add(Cores[Random.RandRange(0, Cores.Num() - 1)]);
        // A reserved card consumes one of the same three slots. Cores never leak into early stages.
        Pool.RemoveAll([&](const auto& Entry) { return IsKeystone(Entry.RewardId); });
        while (!Pool.IsEmpty() && Result.Num() < 3)
        {
            float Total = 0;
            for (const auto& Entry : Pool) Total += Entry.Weight;
            float Roll = Random.FRand() * Total;
            int32 Selected = Pool.Num() - 1;
            for (int32 Index = 0; Index < Pool.Num(); ++Index)
                if ((Roll -= Pool[Index].Weight) <= 0) { Selected = Index; break; }
            Result.Add(Pool[Selected]);
            const int32 Id = Pool[Selected].RewardId;
            Pool.RemoveAll([&](const auto& Entry) { return Entry.RewardId == Id; });
        }
        return Result;
    }
}
