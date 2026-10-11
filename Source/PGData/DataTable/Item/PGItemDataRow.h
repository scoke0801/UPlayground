#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"
#include "PGShared/Shared/Enum/PGRewardTypes.h"
#include "PGItemOptionDefinition.h"
#include "PGItemDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGItemDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 Id = {};

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 IconPanel = -1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EPGEquipmentSlot Slot = EPGEquipmentSlot::Weapon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EPGEquipmentType EquipmentType = EPGEquipmentType::Standard;
    // Enable only after the corresponding combat and motion path has been validated.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bEnableAdvancedWeapon = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EPGItemRarity Rarity = EPGItemRarity::Common;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="100000")) int32 Power = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<EPGStatType, int32> BaseOptions;
    // Existing definitions retain their original roll until explicitly converted.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation") bool bUseOptionPool = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation", meta=(ClampMin="1", ClampMax="100000")) int32 MinPower = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation", meta=(ClampMin="1", ClampMax="100000")) int32 MaxPower = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation") EPGItemQuality DropQuality = EPGItemQuality::Standard;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation") TArray<FPGItemOptionDefinition> BaseStatRanges;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation") TArray<FPGItemOptionDefinition> ImplicitOptions;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation") TArray<FPGItemOptionDefinition> OptionPool;
    // Index zero means one greater option. Unavailable counts are excluded, not clamped.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation") TArray<float> GreaterCountWeights = {1.f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation", meta=(ClampMin="1")) float GreaterMultiplier = 1.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) int32 RollBonus = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float DropWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<EPGCombatPerk, int32> CombatPerks;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText EffectDescription;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName LegendaryEffect;

    EPGItemRarity GetRarity(const FPGItemInstance& Instance) const { return Instance.ItemVersion > 0 ? Instance.Rarity : Rarity; }

};
