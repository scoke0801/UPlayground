#include "PGSalvageRules.h"
#include "PGEquipmentRules.h"

FPGSalvageRules::FPGSalvageRules()
{
    MaterialNames.Add(TEXT("Shard"), FText::FromString(TEXT("강화 파편")));
    MaterialNames.Add(TEXT("Essence"), FText::FromString(TEXT("전설 정수")));
    for (uint8 Index = 0; Index <= uint8(EPGItemRarity::MythicUnique); ++Index)
    {
        FPGSalvageYield Row;
        Row.Rarity = EPGItemRarity(Index);
        Row.Materials.Add(TEXT("Shard"), 1 + Index * 2);
        Row.PerPowerStep.Add(TEXT("Shard"), 1);
        if (Row.Rarity >= EPGItemRarity::Legendary) Row.Materials.Add(TEXT("Essence"), Index - 3);
        Yields.Add(Row);
    }
}
bool FPGSalvageRules::IsValid() const
{
    if (MaterialNames.IsEmpty()) return false;
    for (const auto& Pair : MaterialNames) if (Pair.Key.IsNone() || Pair.Value.IsEmpty()) return false;
    TSet<EPGItemRarity> Seen;
    for (const auto& Row : Yields)
    {
        if (Row.Rarity > EPGItemRarity::MythicUnique || Seen.Contains(Row.Rarity) || Row.PowerStep < 1 || Row.Materials.IsEmpty()) return false;
        Seen.Add(Row.Rarity);
        for (const auto& Pair : Row.Materials) if (!MaterialNames.Contains(Pair.Key) || Pair.Value <= 0) return false;
        for (const auto& Pair : Row.PerPowerStep) if (!MaterialNames.Contains(Pair.Key) || Pair.Value < 0) return false;
    }
    return Seen.Num() == int32(EPGItemRarity::MythicUnique) + 1;
}
bool FPGSalvageRules::GetYield(const FPGItemInstance& Item, TMap<FName, int64>& Out) const
{
    Out.Reset();
    if (!IsValid() || Item.ItemVersion != 1 || !PGEquipmentRules::ValidateInstance(Item)) return false;
    // Imprinting never increases salvage returns; use the original rarity.
    const auto* Row = Yields.FindByPredicate([&](const auto& R){ return R.Rarity == Item.OriginalRarity; });
    if (!Row) return false;
    for (const auto& Pair : Row->Materials) Out.Add(Pair.Key, Pair.Value);
    for (const auto& Pair : Row->PerPowerStep)
        Out.FindOrAdd(Pair.Key) += int64((Item.Power - 1) / Row->PowerStep) * Pair.Value;
    return true;
}
