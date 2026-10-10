#pragma once
#include "CoreMinimal.h"
#include "PGDungeonTypes.generated.h"

UENUM(BlueprintType)
enum class EPGDungeonRoomRole : uint8 { Entrance, Combat, Elite, Treasure, Boss };
UENUM(BlueprintType)
enum class EPGDungeonState : uint8 { Idle, Structure, Assembly, Navigation, Ready, Failed, Cancelled, Environment };

USTRUCT(BlueprintType)
struct PGSHARED_API FPGDungeonRoom
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Id = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FIntPoint Cell = FIntPoint::ZeroValue;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EPGDungeonRoomRole Role = EPGDungeonRoomRole::Combat;
    // 1..5 are mandatory combat objectives; 0 denotes a non-objective room.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Objective = 0;
};
USTRUCT(BlueprintType)
struct PGSHARED_API FPGDungeonConnection
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 A = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 B = 0;
};
USTRUCT(BlueprintType)
struct PGSHARED_API FPGDungeonLayout
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Version = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Seed = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Attempt = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bFallback = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FPGDungeonRoom> Rooms;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FPGDungeonConnection> Connections;
};
