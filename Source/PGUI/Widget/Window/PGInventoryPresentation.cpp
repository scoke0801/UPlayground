#include "PGInventoryPresentation.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"

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

FText PGInventoryPresentation::StatValue(EPGStatType Stat, float Value, bool bDelta)
{
    FString Number;
    if (Stat == EPGStatType::CriticalRate)
        Number = FString::Printf(TEXT("%.2f%s"), Value * .01f, bDelta ? TEXT("%p") : TEXT("%"));
    else
    {
        FNumberFormattingOptions Format;
        Format.SetMaximumFractionalDigits(2);
        Number = FText::AsNumber(Value, &Format).ToString();
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
    case EPGItemRarity::Epic: return NSLOCTEXT("PGInventory", "Epic", "영웅");
    case EPGItemRarity::Legendary: return NSLOCTEXT("PGInventory", "Legendary", "전설");
    case EPGItemRarity::Unique: return NSLOCTEXT("PGInventory", "Unique", "고유");
    case EPGItemRarity::MythicUnique: return NSLOCTEXT("PGInventory", "MythicUnique", "신화 고유");
    case EPGItemRarity::Rare: return NSLOCTEXT("PGInventory", "Rare", "희귀");
    case EPGItemRarity::Magic: return NSLOCTEXT("PGInventory", "Magic", "마법");
    default: return NSLOCTEXT("PGInventory", "Common", "일반");
    }
}

FText PGInventoryPresentation::SlotName(EPGEquipmentSlot Slot)
{
    static const TCHAR* Names[] = {TEXT("주무기"), TEXT("목걸이"), TEXT("머리"), TEXT("몸통"), TEXT("손"), TEXT("다리"), TEXT("발"), TEXT("반지 왼쪽"), TEXT("반지 오른쪽"), TEXT("보조무기")};
    return FText::FromString(uint8(Slot) < UE_ARRAY_COUNT(Names) ? Names[uint8(Slot)] : TEXT("알 수 없는 부위"));
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


TArray<FGuid> PGInventoryPresentation::BagItems(const TArray<FPGItemInstance>& Items,
    const TMap<EPGEquipmentSlot, FGuid>& Equipment, const UPGProgressionData& Catalog,
    const FPGInventoryFilter& Filter, EPGInventorySort Sort)
{
    TArray<int32> Indices;
    for (int32 I = 0; I < Items.Num(); ++I)
    {
        const auto& Item = Items[I];
        const auto* Def = Catalog.FindItem(Item.DefinitionId);
        if (Equipment.FindKey(Item.Guid)) continue;
        const int32 Slot = Def ? int32(Def->Slot) : -1;
        const int32 Rarity = Def ? int32(Def->GetRarity(Item)) : int32(Item.Rarity);
        if (Filter.Slot >= 0 && Slot != Filter.Slot &&
            !(Filter.Slot == int32(EPGEquipmentSlot::RingLeft) && Slot == int32(EPGEquipmentSlot::RingRight))) continue;
        if (Filter.Rarity >= 0 && Rarity != Filter.Rarity) continue;
        if (Filter.Quality >= 0 && int32(Item.Quality) != Filter.Quality) continue;
        if ((Filter.Mark == 1 && !Item.bFavorite) || (Filter.Mark == 2 && !Item.bJunk)) continue;
        Indices.Add(I);
    }
    Indices.Sort([&](int32 A, int32 B)
    {
        const auto& L = Items[A]; const auto& R = Items[B];
        const auto* LD = Catalog.FindItem(L.DefinitionId); const auto* RD = Catalog.FindItem(R.DefinitionId);
        if (Sort == EPGInventorySort::Slot)
        {
            const int32 LS = LD ? int32(LD->Slot) : int32(EPGEquipmentSlot::Max);
            const int32 RS = RD ? int32(RD->Slot) : int32(EPGEquipmentSlot::Max);
            if (LS != RS) return LS < RS;
        }
        if (Sort == EPGInventorySort::Rarity)
        {
            const auto LR = LD ? LD->GetRarity(L) : L.Rarity;
            const auto RR = RD ? RD->GetRarity(R) : R.Rarity;
            if (LR != RR) return LR > RR;
        }
        if (Sort == EPGInventorySort::Power && L.Power != R.Power) return L.Power > R.Power;
        // Items is append-only on acquisition; sorting never mutates saved ownership order.
        return A > B;
    });
    TArray<FGuid> Result;
    for (int32 I : Indices) Result.Add(Items[I].Guid);
    return Result;
}


bool PGInventoryPresentation::PreviewEquip(const TArray<FPGItemInstance>& Items,
    const TMap<EPGEquipmentSlot,FGuid>& Equipment, const UPGProgressionData& Catalog,
    FGuid Guid, EPGEquipmentSlot Target, TArray<FPGInventoryStatComparison>& Rows, FString& Reason)
{
    Rows.Reset();
    TMap<EPGEquipmentSlot,FGuid> Next;
    if (!PGEquipmentRules::PlanEquip(Catalog,Items,Equipment,Guid,Target,Next,Reason)) return false;
    FPGItemInstance Before, After;
    for (const auto& Item : Items)
        for (const auto& Option : Item.Options)
        {
            if (Equipment.FindKey(Item.Guid)) Before.Options.FindOrAdd(Option.Key)+=Option.Value;
            if (Next.FindKey(Item.Guid)) After.Options.FindOrAdd(Option.Key)+=Option.Value;
        }
    Rows=Compare(After,&Before);
    return true;
}
