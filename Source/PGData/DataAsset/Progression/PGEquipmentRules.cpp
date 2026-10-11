#include "PGEquipmentRules.h"
#include "PGProgressionData.h"

TArray<FPGEquipmentRarityRule> PGEquipmentRules::DefaultRarities()
{
    TArray<FPGEquipmentRarityRule> Result;
    for (uint8 Index = 0; Index <= uint8(EPGItemRarity::MythicUnique); ++Index)
    {
        FPGEquipmentRarityRule Rule;
        Rule.Rarity = EPGItemRarity(Index);
        Rule.OptionCount = Index < 3 ? Index : Index < 5 ? 3 : 4;
        Rule.bFixedOptions = Index >= 5;
        Rule.bAllowAncestral = Index >= 3;
        Rule.bAllowTemper = Index >= 2 && Index <= 4;
        Rule.bAllowImprint = Index == 3 || Index == 4;
        Rule.MaxMasterwork = Index < 2 ? 0 : Index == 2 ? 3 : 5;
        Rule.bDropEnabled = Index <= 3;
        Result.Add(Rule);
    }
    return Result;
}

bool PGEquipmentRules::ValidateRarities(const TArray<FPGEquipmentRarityRule>& Rules, FString& Error)
{
    TSet<EPGItemRarity> Seen;
    for (const auto& Rule : Rules)
    {
        if (Rule.Rarity > EPGItemRarity::MythicUnique || Seen.Contains(Rule.Rarity) ||
            Rule.OptionCount < 0 || Rule.OptionCount > 4 || Rule.MaxMasterwork < 0 || Rule.MaxMasterwork > 5 ||
            (Rule.bAllowAncestral && Rule.Rarity < EPGItemRarity::Epic))
        { Error = TEXT("장비 등급 규칙의 중복 또는 범위 오류"); return false; }
        Seen.Add(Rule.Rarity);
    }
    if (Seen.Num() != 7) { Error = TEXT("장비 등급 규칙 7종이 필요합니다"); return false; }
    return true;
}

bool PGEquipmentRules::ValidateInstance(const FPGItemInstance& Item)
{
    if (!Item.Guid.IsValid() || Item.DefinitionId <= 0 || Item.ItemVersion < 0 || Item.ItemVersion > 1) return false;
    for (const auto& Pair : Item.Options)
        if (Pair.Key <= EPGStatType::None || Pair.Key >= EPGStatType::Max || Pair.Value < 0 || Pair.Value > 110000) return false;
    if (Item.MasterworkLevel < 0 || Item.MasterworkLevel > (Item.Rarity == EPGItemRarity::Rare ? 3 : 5) ||
        ((Item.MasterworkLevel == 0) != Item.MasterworkBonuses.IsEmpty()) ||
        (Item.MasterworkLevel > 0 && (Item.Rarity < EPGItemRarity::Rare || Item.OptionRolls.IsEmpty()))) return false;
    if (Item.Temper.Id.IsNone())
    {
        if (Item.Temper.Stat != EPGStatType::None || Item.Temper.Value != 0 || Item.Temper.Min != 0 || Item.Temper.Max != 0 || Item.Temper.bGreater) return false;
    }
    else if (Item.Rarity < EPGItemRarity::Rare || Item.Rarity > EPGItemRarity::Legendary || Item.Temper.bGreater ||
        Item.Temper.Stat <= EPGStatType::None || Item.Temper.Stat >= EPGStatType::Max || Item.Temper.Value <= 0 ||
        Item.Temper.Value > 100000 || Item.Temper.Min != Item.Temper.Value || Item.Temper.Max != Item.Temper.Value ||
        Item.Options.FindRef(Item.Temper.Stat) < Item.Temper.Value) return false;
    for (const auto& Pair : Item.MasterworkBonuses)
        if (Pair.Value <= 0 || Pair.Value > 110000 || !Item.OptionRolls.ContainsByPredicate([&](const auto& Roll)
            { return Roll.Source == EPGItemOptionSource::Base && Roll.Stat == Pair.Key && Roll.Value > 0; })) return false;
    if (Item.ItemVersion == 0) return Item.Temper.Id.IsNone() && Item.MasterworkLevel == 0;
    if (Item.Rarity > EPGItemRarity::MythicUnique || Item.OriginalRarity > EPGItemRarity::MythicUnique ||
        Item.Quality > EPGItemQuality::Ancestral || Item.Power < 1 || Item.Power > 100000) return false;
    if (Item.Quality == EPGItemQuality::Ancestral && (Item.Rarity < EPGItemRarity::Epic || Item.GreaterOptions.IsEmpty())) return false;
    if (Item.Quality == EPGItemQuality::Standard && !Item.GreaterOptions.IsEmpty()) return false;
    for (auto Stat : Item.GreaterOptions) if (!Item.Options.Contains(Stat)) return false;
    if (!Item.OptionRolls.IsEmpty())
    {
        TMap<EPGStatType, int32> Totals;
        TSet<FName> Ids;
        TSet<EPGStatType> Affixes, Greater;
        for (const auto& Roll : Item.OptionRolls)
        {
            if (Roll.Id.IsNone() || Ids.Contains(Roll.Id) || Roll.Stat <= EPGStatType::None || Roll.Stat >= EPGStatType::Max ||
                Roll.Source > EPGItemOptionSource::Affix || Roll.Min < 0 || Roll.Max < Roll.Min || Roll.Max > 100000 ||
                Roll.Value < Roll.Min || Roll.Value > 100000 || (!Roll.bGreater && Roll.Value > Roll.Max) ||
                (Roll.bGreater && (Roll.Source != EPGItemOptionSource::Affix || Roll.Value < Roll.Max))) return false;
            if (Roll.Source == EPGItemOptionSource::Affix)
            {
                if (Affixes.Contains(Roll.Stat)) return false;
                Affixes.Add(Roll.Stat);
            }
            Ids.Add(Roll.Id);
            int32& Total = Totals.FindOrAdd(Roll.Stat);
            if (Roll.Value > 110000 - Total) return false;
            Total += Roll.Value;
            if (Roll.bGreater) Greater.Add(Roll.Stat);
        }
        if (!Item.Temper.Id.IsNone()) Totals.FindOrAdd(Item.Temper.Stat) += Item.Temper.Value;
        for (const auto& Pair : Item.MasterworkBonuses) Totals.FindOrAdd(Pair.Key) += Pair.Value;
        if (!Totals.OrderIndependentCompareEqual(Item.Options) || Greater.Num() != Item.GreaterOptions.Num()) return false;
        for (auto Stat : Greater) if (!Item.GreaterOptions.Contains(Stat)) return false;
    }
    return true;
}


bool PGEquipmentRules::FitsSlot(const FPGItemDataRow& Def, EPGEquipmentSlot Slot)
{
    if (Slot >= EPGEquipmentSlot::Max) return false;
    const bool Ring = Def.Slot == EPGEquipmentSlot::RingLeft || Def.Slot == EPGEquipmentSlot::RingRight;
    return Ring ? Slot == EPGEquipmentSlot::RingLeft || Slot == EPGEquipmentSlot::RingRight : Slot == Def.Slot;
}

int32 PGEquipmentRules::BagCount(const TArray<FPGItemInstance>& Items, const TMap<EPGEquipmentSlot, FGuid>& Equipment)
{
    int32 Count = 0;
    for (const auto& Item : Items) if (!Equipment.FindKey(Item.Guid)) ++Count;
    return Count;
}

bool PGEquipmentRules::PlanEquip(const UPGProgressionData& Catalog, const TArray<FPGItemInstance>& Items,
    const TMap<EPGEquipmentSlot, FGuid>& Equipment, FGuid Guid, EPGEquipmentSlot Target,
    TMap<EPGEquipmentSlot, FGuid>& Out, FString& Error)
{
    Out = Equipment;
    const auto* Item = Items.FindByPredicate([&](const auto& I){ return I.Guid == Guid; });
    const auto* Def = Item ? Catalog.FindItem(Item->DefinitionId) : nullptr;
    if (!Def || !FitsSlot(*Def, Target)) { Error = TEXT("이 부위에 장착할 수 없습니다"); return false; }
    const auto Type = Def->EquipmentType;
    if (Type > EPGEquipmentType::Focus || (Type != EPGEquipmentType::Standard && Type != EPGEquipmentType::OneHandSword &&
        Type != EPGEquipmentType::Shield && !Def->bEnableAdvancedWeapon))
    { Error = TEXT("아직 지원하지 않는 무기 유형입니다"); return false; }
    if (((Type == EPGEquipmentType::OneHandSword || Type == EPGEquipmentType::TwoHandSword) && Target != EPGEquipmentSlot::Weapon) ||
        ((Type == EPGEquipmentType::Shield || Type == EPGEquipmentType::Focus) && Target != EPGEquipmentSlot::OffHand))
    { Error = TEXT("무기 유형과 부위가 일치하지 않습니다"); return false; }
    if (Def->GetRarity(*Item) >= EPGItemRarity::Unique)
        for (const auto& Other : Items)
            if (Other.Guid != Guid && Other.DefinitionId == Item->DefinitionId && Equipment.FindKey(Other.Guid) &&
                *Equipment.FindKey(Other.Guid) != Target)
            { Error = TEXT("같은 고유 장비는 하나만 장착할 수 있습니다"); return false; }
    auto Next = Equipment;
    for (auto It = Next.CreateIterator(); It; ++It) if (It.Value() == Guid) It.RemoveCurrent();
    if (Target == EPGEquipmentSlot::OffHand)
    {
        const auto Main = Next.FindRef(EPGEquipmentSlot::Weapon);
        const auto* MainItem = Items.FindByPredicate([&](const auto& I){ return I.Guid == Main; });
        const auto* MainDef = MainItem ? Catalog.FindItem(MainItem->DefinitionId) : nullptr;
        if (MainDef && MainDef->EquipmentType == EPGEquipmentType::TwoHandSword)
        { Error = TEXT("양손 무기를 먼저 해제하세요"); return false; }
    }
    Next.Add(Target, Guid);
    if (Type == EPGEquipmentType::TwoHandSword) Next.Remove(EPGEquipmentSlot::OffHand);
    const int32 Required = BagCount(Items, Next) - Catalog.BagCapacity;
    if (Required > 0) { Error = FString::Printf(TEXT("가방에 빈 칸 %d개가 더 필요합니다"), Required); return false; }
    Out = MoveTemp(Next); Error.Reset(); return true;
}
