#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"
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
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EPGEquipmentSlot Slot = EPGEquipmentSlot::Weapon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EPGItemRarity Rarity = EPGItemRarity::Common;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<EPGStatType, int32> BaseOptions;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) int32 RollBonus = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float DropWeight = 1.f;

};
