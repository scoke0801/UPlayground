#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGData/DataTable/Item/PGItemDataRow.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGProgressionData.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGLoadoutEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) EPGSkillSlot Slot = EPGSkillSlot::NormalAttack;
    UPROPERTY(EditAnywhere) int32 SkillId = 0;
    // -1 inherits the skill row; a nonnegative value is an authored loadout cooldown.
    UPROPERTY(EditAnywhere, meta=(ClampMin="-1")) float CooldownSeconds = -1.f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0.1")) float CooldownScale = 1.f;
};
USTRUCT(BlueprintType)
struct PGDATA_API FPGBuildDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName Id;
    UPROPERTY(EditAnywhere) FText DisplayName;
    UPROPERTY(EditAnywhere) int32 RequiredClears = 0;
    UPROPERTY(EditAnywhere) TArray<FPGLoadoutEntry> Skills;
};
UCLASS(BlueprintType)
class PGDATA_API UPGProgressionData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Loot Presentation", meta=(ClampMin="0.05")) float DropArcDuration = .45f;
    UPROPERTY(EditAnywhere, Category="Loot Presentation", meta=(ClampMin="0")) float DropArcHeight = 100.f;
    UPROPERTY(EditAnywhere, Category="Loot Presentation") TMap<EPGItemRarity, TSoftObjectPtr<class UNiagaraSystem>> DropBeams;
    UPROPERTY(EditAnywhere, Category="Loot Presentation") TMap<EPGItemRarity, TSoftObjectPtr<class USoundBase>> DropSounds;
    UPROPERTY(EditAnywhere, Category="Loot Presentation") TSoftObjectPtr<class UMaterialInterface> BeamMaterial;
    UPROPERTY(EditAnywhere, Category="Loot Presentation") TMap<EPGItemRarity, float> BeamHeights;
    UPROPERTY(EditAnywhere) int32 BagCapacity = 24;
    UPROPERTY(EditAnywhere) float PickupRadius = 250.f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="1")) float DropChance = 0.75f;
    UPROPERTY(EditAnywhere) TArray<FPGItemDataRow> Items;
    UPROPERTY(EditAnywhere) TArray<FPGBuildDefinition> Builds;
    const FPGItemDataRow* FindItem(int32 Id) const { return Items.FindByPredicate([Id](const auto& I){ return I.Id == Id; }); }
};
