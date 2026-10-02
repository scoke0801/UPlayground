#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "UObject/SoftObjectPath.h"
#include "PGShared/Shared/Enum/PGRewardTypes.h"
#include "PGRewardStatDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGRewardStatDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 StatId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward")
    EPGStatType StatType = EPGStatType::Health;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward", meta=(ClampMin="0"))
    int32 Amount = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward")
    EPGRewardGrade Grade = EPGRewardGrade::Normal;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward")
    TSoftObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward")
    EPGCombatPerk Perk = EPGCombatPerk::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward", meta=(ClampMin="0", ClampMax="100"))
    int32 PerkPercent = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward", meta=(MultiLine="true"))
    FText PlaystyleDescription;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward", meta=(ClampMin="-1", ClampMax="2"))
    int32 IconPanel = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roguelike")
    EPGCombatPerk RequiredPerk = EPGCombatPerk::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roguelike")
    FName BuildFamily;
    // Zero preserves legacy repeatable rewards.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roguelike", meta=(ClampMin="0"))
    int32 MaxSelections = 0;
    // RequiredPerk remains an additional AND condition for existing assets.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roguelike")
    TArray<EPGCombatPerk> RequiredPerks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roguelike")
    TArray<EPGCombatPerk> RequiredAnyPerks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roguelike")
    bool bKeystone = false;

    bool MeetsRequirements(TFunctionRef<int32(EPGCombatPerk)> GetPerk) const
    {
        if (RequiredPerk != EPGCombatPerk::None && GetPerk(RequiredPerk) <= 0) return false;
        for (const auto Required : RequiredPerks) if (GetPerk(Required) <= 0) return false;
        return RequiredAnyPerks.IsEmpty() || RequiredAnyPerks.ContainsByPredicate([&](EPGCombatPerk Required) { return GetPerk(Required) > 0; });
    }
};
