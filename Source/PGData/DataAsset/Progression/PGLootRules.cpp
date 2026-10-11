#include "PGLootRules.h"
#include "PGItemGeneration.h"

bool PGLootRules::ValidatePools(const UPGProgressionData& Catalog, FString& Error)
{
    TSet<int32> Definitions;
    for (const auto& Item : Catalog.Items)
    {
        if (Definitions.Contains(Item.Id) || !FMath::IsFinite(Item.DropWeight) || Item.DropWeight < 0.f)
        { Error = TEXT("장비 정의 중복/가중치 오류"); return false; }
        Definitions.Add(Item.Id);
        const auto* Rule = Catalog.RarityRules.FindByPredicate([&](const auto& R){ return R.Rarity == Item.Rarity; });
        if (!Rule) { Error = TEXT("장비 등급 규칙 누락"); return false; }
        if (!PGItemGeneration::Validate(Item, *Rule, Error)) return false;
    }
    if (!Catalog.InvalidEquipmentFallbackPool.IsNone() && !Catalog.FindDropPool(Catalog.InvalidEquipmentFallbackPool))
    { Error = TEXT("장비 대체 드랍 풀 누락"); return false; }
    TSet<FName> Ids;
    for (const auto& Pool : Catalog.DropPools)
    {
        if (Pool.Id.IsNone() || Ids.Contains(Pool.Id) || !FMath::IsFinite(Pool.DropChance) || Pool.DropChance < 0.f || Pool.DropChance > 1.f)
        { Error = TEXT("드랍 풀 ID/확률 오류"); return false; }
        Ids.Add(Pool.Id);
        TSet<int32> Items;
        double Total = 0.;
        for (const auto& Entry : Pool.Entries)
        {
            if (!Catalog.FindItem(Entry.ItemId) || Items.Contains(Entry.ItemId) || !FMath::IsFinite(Entry.Weight) || Entry.Weight < 0.f)
            { Error = FString::Printf(TEXT("드랍 풀 %s: 아이템/가중치 오류"), *Pool.Id.ToString()); return false; }
            Items.Add(Entry.ItemId); Total += Entry.Weight;
        }
        if (Total <= 0. || !FMath::IsFinite(Total) || Total > MAX_flt)
        { Error = FString::Printf(TEXT("드랍 풀 %s: 선택 가능한 장비 없음"), *Pool.Id.ToString()); return false; }
    }
    return true;
}

static bool RollInternal(const UPGProgressionData& Catalog, FName PoolId, FRandomStream& Random, FPGItemInstance& Out, bool AllowFallback)
{
    Out = FPGItemInstance();
    const auto* Pool = PoolId.IsNone() ? nullptr : Catalog.FindDropPool(PoolId);
    if (!PoolId.IsNone() && !Pool) return false;
    const float Chance = Pool ? Pool->DropChance : Catalog.DropChance;
    if ((!Pool || !Pool->bGuaranteed) && Random.FRand() >= Chance) return false;
    TArray<FPGDropPoolEntry> Entries;
    if (Pool) Entries = Pool->Entries;
    else for (const auto& Item : Catalog.Items) Entries.Add({Item.Id, Item.DropWeight});
    // Authoring order and TMap iteration cannot change an established roll.
    Entries.Sort([](const auto& A, const auto& B) { return A.ItemId < B.ItemId; });
    double Total = 0.;
    for (const auto& Entry : Entries)
    {
        if (!FMath::IsFinite(Entry.Weight) || Entry.Weight < 0.f || !Catalog.FindItem(Entry.ItemId)) return false;
        Total += Entry.Weight;
    }
    if (Total <= 0. || !FMath::IsFinite(Total)) return false;
    double Choice = Random.FRand() * Total;
    const FPGItemDataRow* Selected = nullptr;
    for (const auto& Entry : Entries)
    {
        if (Entry.Weight <= 0.f) continue;
        Selected = Catalog.FindItem(Entry.ItemId);
        Choice -= Entry.Weight;
        if (Choice < 0.) break;
    }
    if (!Selected) return false;
    const auto* Rule = Catalog.RarityRules.FindByPredicate([Selected](const auto& Entry){ return Entry.Rarity == Selected->Rarity; });
    if (!Rule || !Rule->bDropEnabled) return false;
    if (Selected->bUseOptionPool)
    {
        FString Error;
        if (PGItemGeneration::Generate(*Selected, *Rule, Random, Out, Error)) return true;
        UE_LOG(LogTemp, Warning, TEXT("Equipment generation rejected: %s"), *Error);
        const auto* Fallback = AllowFallback && PoolId != Catalog.InvalidEquipmentFallbackPool ?
            Catalog.FindDropPool(Catalog.InvalidEquipmentFallbackPool) : nullptr;
        if (!Fallback) return false;
        TSet<int32> Seen;
        double FallbackWeight = 0.;
        if (!FMath::IsFinite(Fallback->DropChance) || Fallback->DropChance < 0.f || Fallback->DropChance > 1.f) return false;
        for (const auto& Entry : Fallback->Entries)
        {
            const auto* Def = Catalog.FindItem(Entry.ItemId);
            const auto* FallbackRule = Def ? Catalog.RarityRules.FindByPredicate([&](const auto& R){ return R.Rarity == Def->Rarity; }) : nullptr;
            if (!Def || !FallbackRule || !FallbackRule->bDropEnabled || Seen.Contains(Entry.ItemId) ||
                !FMath::IsFinite(Entry.Weight) || Entry.Weight < 0.f || !PGItemGeneration::Validate(*Def, *FallbackRule, Error)) return false;
            Seen.Add(Entry.ItemId); FallbackWeight += Entry.Weight;
        }
        if (FallbackWeight <= 0. || FallbackWeight > MAX_flt) return false;
        return RollInternal(Catalog, Fallback->Id, Random, Out, false);
    }
    Out.Guid = FGuid::NewGuid(); Out.DefinitionId = Selected->Id; Out.Options = Selected->BaseOptions;
    Out.ItemVersion = 1; Out.Rarity = Selected->Rarity; Out.OriginalRarity = Selected->Rarity; Out.Power = Selected->Power;
    TArray<EPGStatType> Keys; Out.Options.GetKeys(Keys); Keys.Sort();
    for (auto Key : Keys) Out.Options[Key] += Random.RandRange(0, Selected->RollBonus);
    return true;
}

bool PGLootRules::Roll(const UPGProgressionData& Catalog, FName PoolId, FRandomStream& Random, FPGItemInstance& Out)
{
    if (!RollInternal(Catalog, PoolId, Random, Out, true)) return false;
    Catalog.InitializeLegendary(Out);
    return PGLegendaryRules::ValidateItem(Catalog,Out);
}
