#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"
#include "PGShared/Shared/Enum/PGRewardTypes.h"
#include "PGProfileSave.generated.h"

UCLASS()
class PGACTOR_API UPGProfileSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) int64 Revision = 0;
    UPROPERTY(SaveGame) TArray<FPGItemInstance> Items;
    UPROPERTY(SaveGame) TMap<EPGEquipmentSlot, FGuid> Equipment;
    UPROPERTY(SaveGame) FName BuildId;
    UPROPERTY(SaveGame) int32 Checkpoint = 1;
    UPROPERTY(SaveGame) int32 ClearedStages = 0;
    UPROPERTY(SaveGame) FGuid LastReward;
    UPROPERTY(SaveGame) TMap<EPGStatType, int32> RewardBonuses;
    // Additive version-1 field: older saves deserialize with an empty map.
    UPROPERTY(SaveGame) TMap<EPGCombatPerk, int32> CombatPerks;
};
