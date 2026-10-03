#include "PGInventoryPresentation.h"

FText PGInventoryPresentation::StatName(EPGStatType Stat)
{
    switch (Stat)
    {
    case EPGStatType::Health: return NSLOCTEXT("PGInventory", "Health", "최대 생명력");
    case EPGStatType::Attack: return NSLOCTEXT("PGInventory", "Attack", "공격력");
    case EPGStatType::Defense: return NSLOCTEXT("PGInventory", "Defense", "방어력");
    case EPGStatType::CriticalRate: return NSLOCTEXT("PGInventory", "CritRate", "치명타 확률");
    case EPGStatType::CriticalDamage: return NSLOCTEXT("PGInventory", "CritDamageBonus", "치명타 추가 피해");
    case EPGStatType::HealAmount: return NSLOCTEXT("PGInventory", "Heal", "회복량");
    case EPGStatType::MovementSpeed: return NSLOCTEXT("PGInventory", "MoveSpeed", "이동 속도");
    default: return NSLOCTEXT("PGInventory", "UnknownStat", "알 수 없는 능력치");
    }
}

FText PGInventoryPresentation::StatValue(EPGStatType Stat, int32 Value, bool bDelta)
{
    FString Number;
    if (Stat == EPGStatType::CriticalRate)
        Number = FString::Printf(TEXT("%.2f%s"), Value * .01f, bDelta ? TEXT("%p") : TEXT("%"));
    else
    {
        Number = FText::AsNumber(Value).ToString();
        if (Stat == EPGStatType::CriticalDamage) Number += bDelta ? TEXT("%p") : TEXT("%");
        if (Stat == EPGStatType::MovementSpeed) Number += TEXT(" cm/s");
    }
    if (bDelta && Value > 0) Number = TEXT("+") + Number;
    return FText::FromString(Number);
}

FText PGInventoryPresentation::RarityName(EPGItemRarity Rarity)
{
    switch (Rarity)
    {
    case EPGItemRarity::Rare: return NSLOCTEXT("PGInventory", "Rare", "희귀");
    case EPGItemRarity::Magic: return NSLOCTEXT("PGInventory", "Magic", "마법");
    default: return NSLOCTEXT("PGInventory", "Common", "일반");
    }
}

FText PGInventoryPresentation::SlotName(EPGEquipmentSlot Slot)
{
    return Slot == EPGEquipmentSlot::Weapon ? NSLOCTEXT("PGInventory", "Weapon", "무기") : NSLOCTEXT("PGInventory", "Accessory", "장신구");
}

TArray<FPGInventoryStatComparison> PGInventoryPresentation::Compare(const FPGItemInstance& Selected, const FPGItemInstance* Current)
{
    TArray<FPGInventoryStatComparison> Result;
    // Enum order is stable; include removed options as negative deltas.
    for (uint8 Index = uint8(EPGStatType::None) + 1; Index < uint8(EPGStatType::Max); ++Index)
    {
        const auto Stat = EPGStatType(Index);
        if (Selected.Options.Contains(Stat) || (Current && Current->Options.Contains(Stat)))
            Result.Add({Stat, Current ? Current->Options.FindRef(Stat) : 0, Selected.Options.FindRef(Stat)});
    }
    return Result;
}
