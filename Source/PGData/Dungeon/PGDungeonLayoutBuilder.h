#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Structure/PGDungeonTypes.h"
class UPGDungeonDefinition;
namespace PGDungeon
{
    PGDATA_API int32 StreamSeed(int32 Seed, int32 Salt, int32 RoomId = 0);
    PGDATA_API bool Build(const UPGDungeonDefinition& Definition, int32 Seed, int32 Attempt, FPGDungeonLayout& Out, FString& Error, bool bFallback = false);
    PGDATA_API bool Validate(const UPGDungeonDefinition& Definition, const FPGDungeonLayout& Layout, FString& Error);
}
