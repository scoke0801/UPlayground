#pragma once
#include "CoreMinimal.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGDungeonTreasure.generated.h"

UCLASS()
class PGACTOR_API APGDungeonTreasure : public APGLootDrop
{
    GENERATED_BODY()
public:
    bool InitializeTreasure(int32 Seed, int32 RoomId, int32 ContentVersion, FName Pool);
    virtual bool TryPickup(APawn* Player) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 DungeonRoomId = INDEX_NONE;
    static FGuid RewardGuid(FGuid RunId, int32 RoomId, int32 ContentVersion);
private:
    FGuid RewardRunId;
};
