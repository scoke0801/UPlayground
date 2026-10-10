#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGShared/Shared/Structure/PGDungeonTypes.h"
#include "PGDungeonRoomDefinition.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGDungeonModulePiece
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UStaticMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UMaterialInterface> Material;
    // Non-colliding authored accents. Walkable floors and sockets belong to the common kit.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform Transform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOccluder = true;
};

UCLASS(BlueprintType)
class PGDATA_API UPGDungeonRoomDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ModuleId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EPGDungeonRoomRole Role = EPGDungeonRoomRole::Combat;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float RoomSize = 2800.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<int32> AllowedQuarterTurns = {0};
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FPGDungeonModulePiece> Pieces;
};
