#include "PGLootRules.h"

bool PGLootRules::ValidatePools(const UPGProgressionData& Catalog, FString& Error)
{
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

bool PGLootRules::Roll(const UPGProgressionData& Catalog, FName PoolId, FRandomStream& Random, FPGItemInstance& Out)
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
    Out.Guid = FGuid::NewGuid(); Out.DefinitionId = Selected->Id; Out.Options = Selected->BaseOptions;
    TArray<EPGStatType> Keys; Out.Options.GetKeys(Keys); Keys.Sort();
    for (auto Key : Keys) Out.Options[Key] += Random.RandRange(0, Selected->RollBonus);
    return true;
}
