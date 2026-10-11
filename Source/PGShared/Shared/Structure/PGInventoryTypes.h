#pragma once
#include "CoreMinimal.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGInventoryTypes.generated.h"

UENUM(BlueprintType)
// Serialized legacy names and values remain: Weapon = main hand, Accessory = necklace.
enum class EPGEquipmentSlot : uint8 { Weapon, Accessory, Head, Chest, Hands, Legs, Feet, RingLeft, RingRight, OffHand, Max };
UENUM(BlueprintType)
enum class EPGEquipmentType : uint8 { Standard, OneHandSword, TwoHandSword, Dagger, Shield, Focus };
UENUM(BlueprintType)
enum class EPGItemRarity : uint8 { Common, Magic, Rare, Epic, Legendary, Unique, MythicUnique };

UENUM(BlueprintType)
enum class EPGItemQuality : uint8 { Standard, Ancestral };

UENUM(BlueprintType)
enum class EPGItemOptionSource : uint8 { Base, Implicit, Affix };

USTRUCT(BlueprintType)
struct PGSHARED_API FPGItemOptionRoll
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) FName Id;
    UPROPERTY(SaveGame, BlueprintReadOnly) EPGStatType Stat = EPGStatType::None;
    UPROPERTY(SaveGame, BlueprintReadOnly) EPGItemOptionSource Source = EPGItemOptionSource::Base;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 Value = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 Min = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 Max = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) bool bGreater = false;
};

USTRUCT(BlueprintType)
struct PGSHARED_API FPGItemInstance
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) FPGItemOptionRoll Temper;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 MasterworkLevel = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) TMap<EPGStatType, int32> MasterworkBonuses;
    UPROPERTY(SaveGame, BlueprintReadOnly) FName LegendaryEffect;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 LegendaryPercent = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid Guid;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 DefinitionId = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) TMap<EPGStatType, int32> Options;
    // Zero identifies legacy instances; migration never rerolls Options.
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 ItemVersion = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) EPGItemRarity Rarity = EPGItemRarity::Common;
    UPROPERTY(SaveGame, BlueprintReadOnly) EPGItemRarity OriginalRarity = EPGItemRarity::Common;
    UPROPERTY(SaveGame, BlueprintReadOnly) EPGItemQuality Quality = EPGItemQuality::Standard;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 Power = 1;
    UPROPERTY(SaveGame, BlueprintReadOnly) TSet<EPGStatType> GreaterOptions;
    UPROPERTY(SaveGame, BlueprintReadOnly) bool bFavorite = false;
    UPROPERTY(SaveGame, BlueprintReadOnly) bool bJunk = false;
    // Optional provenance; legacy instances keep their already resolved Options untouched.
    UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FPGItemOptionRoll> OptionRolls;
};
