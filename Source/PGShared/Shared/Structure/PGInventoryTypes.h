#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGInventoryTypes.generated.h"

UENUM(BlueprintType)
enum class EPGEquipmentSlot : uint8 { Weapon, Accessory };
UENUM(BlueprintType)
enum class EPGItemRarity : uint8 { Common, Magic, Rare };

USTRUCT(BlueprintType)
struct PGSHARED_API FPGItemInstance
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid Guid;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 DefinitionId = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) TMap<EPGStatType, int32> Options;
};
