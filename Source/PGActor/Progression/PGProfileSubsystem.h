#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PGProfileSave.h"
#include "PGProfileSubsystem.generated.h"
class UPGProgressionData;
class APGCharacterPlayer;

UCLASS()
class PGACTOR_API UPGProfileSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    static UPGProfileSubsystem* Get(const UObject* Context);
    const UPGProfileSave* GetProfile() const { return Profile; }
    const UPGProgressionData* GetCatalog() const { return Catalog; }
    bool TryPickup(const FPGItemInstance& Item);
    bool Equip(FGuid Guid);
    bool Unequip(EPGEquipmentSlot Slot);
    bool Discard(FGuid Guid);
    bool SelectBuild(FName Id);
    bool CommitReward(FGuid Token, int32 NextStage, EPGStatType Stat, int32 Amount, EPGCombatPerk Perk = EPGCombatPerk::None, int32 PerkPercent = 0);
    bool BeginNewRun();
    bool RecoverSave();
    bool IsSaveBlocked() const { return bReadOnly; }
    bool RestorePlayer(APGCharacterPlayer* Player);
    bool RollDrop(FRandomStream& Random, FPGItemInstance& Out) const;
    bool Validate(const UPGProfileSave* Candidate) const;
    bool ValidateCatalog(FString& Error) const;
    FString Status;
    bool bInjectSaveFailure = false;
    int32 RetryProbeRemaining = -1;
    int32 RetryProbeFailures = 0;
private:
    friend class FPGProfileTest;
    UPROPERTY() TObjectPtr<UPGProfileSave> Profile;
    UPROPERTY() TObjectPtr<UPGProgressionData> Catalog;
    bool bReadOnly = false;
    int32 ActiveSlot = -1;
    FString TestSlotPrefix;
    UPROPERTY() TArray<TObjectPtr<UObject>> PreparedSkillAssets;
    bool Commit(UPGProfileSave* Candidate);
    void LoadProfile();
    void RefreshPlayer();
    FString SlotName(int32 Index) const;
};
