#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PGShared/Shared/Structure/PGDungeonTypes.h"
#include "PGDungeonDiscoverySubsystem.generated.h"

// A presentation snapshot deliberately contains only visited rooms and known links.
struct PGACTOR_API FPGDungeonMapSnapshot
{
    TArray<FPGDungeonRoom> Rooms;
    TArray<FPGDungeonConnection> Connections;
    // Doorway stubs expose a visited room's exits, never the destination or its role.
    TArray<TPair<FVector2D, FVector2D>> Doorways;
    int32 CurrentRoom = INDEX_NONE;
    int32 Objective = 0;
    bool bActive = false;
    bool bHasRewards = false;
    TSet<int32> ClaimedTreasures;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FPGDungeonMapChanged, const FPGDungeonMapSnapshot&);

UCLASS()
class PGACTOR_API UPGDungeonDiscoverySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void InitializeMap(const FPGDungeonLayout& InLayout, FVector InOrigin, float InRoomSize, float InSpacing);
    void ResetMap();
    void Observe(FVector Position, int32 Objective);
    void EnableRewards(bool bEnabled);
    void SetTreasureClaimed(int32 RoomId);
    UFUNCTION(BlueprintPure, Category="Dungeon|Map") int32 GetDiscoveredRoomCount() const { return Snapshot.Rooms.Num(); }
    UFUNCTION(BlueprintPure, Category="Dungeon|Map") int32 GetKnownConnectionCount() const { return Snapshot.Connections.Num(); }
    const FPGDungeonMapSnapshot& GetSnapshot() const { return Snapshot; }
    FVector2D ToMapPosition(FVector Position) const;
    float GetRoomRatio() const { return RoomSize / Spacing; }
    FPGDungeonMapChanged OnMapChanged;
private:
    FPGDungeonLayout Layout;
    FPGDungeonMapSnapshot Snapshot;
    TSet<int32> Discovered;
    FVector Origin = FVector::ZeroVector;
    float RoomSize = 1.f;
    float Spacing = 1.f;
};
