#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGDungeonDefinition.generated.h"

class UStaticMesh;
class UMaterialInterface;

// P0 kit contract: square rooms with four centered sockets, no mirroring/elevation.
UCLASS(BlueprintType)
class PGDATA_API UPGDungeonDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Modules") TArray<TSoftObjectPtr<class UPGDungeonRoomDefinition>> RoomModules;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(AllowedClasses="/Script/PCG.PCGGraph")) TSoftObjectPtr<UObject> DecorationGraph;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0", ClampMax="32")) int32 ExteriorAttemptsPerRoom = 16;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="1", ClampMax="120")) float EnvironmentTimeout = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0", ClampMax="1")) float OccludedOpacity = .12f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0.1", ClampMax="1")) float OcclusionFadeSeconds = .18f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rewards") FName BranchLootPool = TEXT("DungeonTreasure");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rewards") TSoftObjectPtr<UStaticMesh> TreasureMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rewards") TSoftObjectPtr<UMaterialInterface> TreasureMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gates") TSoftObjectPtr<UStaticMesh> GateMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gates") TSoftObjectPtr<UMaterialInterface> GateMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gates", meta=(ClampMin="0.1")) float GateTransitionSeconds = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gates") TSoftObjectPtr<class USoundBase> GateSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="200")) float CombatEntryInset = 350.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="200")) float CombatSpawnInset = 500.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="1")) float UnreachableEnemyTimeout = 15.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon") int32 ContentVersion = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="8", ClampMax="12")) int32 MinRooms = 8;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="8", ClampMax="12")) int32 MaxRooms = 12;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="2000")) float RoomSize = 2800.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="600")) float CorridorLength = 1000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="600")) float CorridorWidth = 800.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="100")) float WallHeight = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Theme") TSoftObjectPtr<UStaticMesh> BlockMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Theme") TSoftObjectPtr<UMaterialInterface> GroundMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Theme") TSoftObjectPtr<UMaterialInterface> WallMaterial;
    // Source materials are preserved; decoration is non-colliding and outside sockets.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Theme") TArray<TSoftObjectPtr<UStaticMesh>> RuinMeshes;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Theme") TArray<TSoftObjectPtr<UStaticMesh>> ExteriorMeshes;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Theme", meta=(ClampMin="0", ClampMax="8")) int32 PropsPerRoom = 4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation", meta=(ClampMin="1", ClampMax="3")) int32 MaxAttempts = 3;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation", meta=(ClampMin="1", ClampMax="120")) float NavigationTimeout = 30.f;
};
