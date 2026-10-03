#pragma once
#include "CoreMinimal.h"

// Independent streams keep reward cards unaffected by spawn retries/AI scheduling.
namespace PGRunRandom
{
inline int32 Seed(int32 RunSeed, int32 Stage, int32 Wave, uint32 Channel, int32 Choice = 0)
{
    uint32 Hash = 2166136261u;
    for (uint32 Value : {uint32(RunSeed), uint32(Stage), uint32(Wave), Channel, uint32(Choice)})
        Hash = (Hash ^ Value) * 16777619u;
    return int32(Hash & 0x7fffffffu);
}

inline int32 LootSeed(int32 RunSeed, int32 Stage, int32 Wave, int32 EnemyId, int32 Ordinal)
{
    return Seed(Seed(RunSeed, Stage, Wave, 3, EnemyId), Stage, Wave, 4, Ordinal);
}

inline FGuid LootGuid(const FGuid& RunId, int32 Stage, int32 Wave, int32 EnemyId, int32 Ordinal)
{
    return FGuid::NewDeterministicGuid(FString::Printf(TEXT("PG/Loot/%s/%d/%d/%d/%d"),
        *RunId.ToString(), Stage, Wave, EnemyId, Ordinal));
}
}
