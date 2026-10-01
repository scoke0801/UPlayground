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
}
