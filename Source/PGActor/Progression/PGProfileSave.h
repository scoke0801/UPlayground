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
    // Additive v1 field; an empty identity retains the original player appearance.
    UPROPERTY(SaveGame) FName CharacterId;
    // Empty retains preset choices; legacy pairs gain default extra slots when applied.
    UPROPERTY(SaveGame) TArray<int32> CustomActiveSkills;
    UPROPERTY(SaveGame) int32 Checkpoint = 1;
    UPROPERTY(SaveGame) int32 ClearedStages = 0;
    UPROPERTY(SaveGame) FGuid LastReward;
    UPROPERTY(SaveGame) TMap<EPGStatType, int32> RewardBonuses;
    // Additive version-1 field: older saves deserialize with an empty map.
    UPROPERTY(SaveGame) TMap<EPGCombatPerk, int32> CombatPerks;
    UPROPERTY(SaveGame) TMap<int32, int32> SelectedRewards;
    UPROPERTY(SaveGame) TMap<int32, int32> StageRewardCounts;
    UPROPERTY(SaveGame) int32 CompletedRuns = 0;
    UPROPERTY(SaveGame) int32 BestStage = 0;
    UPROPERTY(SaveGame) bool bRunEnded = false;
    // Additive v1 migration: 0 means an older save; assign and commit before play.
    UPROPERTY(SaveGame) int32 RunSeed = 0;
    UPROPERTY(SaveGame) bool bAssistedRun = false;
    // Additive v1 fields. Old saves get a committed identity before spawning loot.
    UPROPERTY(SaveGame) FGuid RunId;
    UPROPERTY(SaveGame) TSet<FGuid> ClaimedLoot;
    // Run-only result slot: independent of bag capacity, committed with victory.
    UPROPERTY(SaveGame) FPGItemInstance BossReward;
};
