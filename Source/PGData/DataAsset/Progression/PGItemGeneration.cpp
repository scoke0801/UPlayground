#include "PGItemGeneration.h"

namespace
{
    bool AllowsSlot(const FPGItemOptionDefinition& Option, EPGEquipmentSlot Slot)
    {
        if (Option.AllowedSlots.Contains(Slot)) return true;
        return (Slot == EPGEquipmentSlot::RingRight && Option.AllowedSlots.Contains(EPGEquipmentSlot::RingLeft)) ||
            (Slot == EPGEquipmentSlot::RingLeft && Option.AllowedSlots.Contains(EPGEquipmentSlot::RingRight));
    }

    TArray<const FPGItemOptionDefinition*> Candidates(const FPGItemDataRow& Def)
    {
        TArray<const FPGItemOptionDefinition*> Result;
        for (const auto& Option : Def.OptionPool)
            if (Option.bEnabled && Option.Weight > 0.f && AllowsSlot(Option, Def.Slot)) Result.Add(&Option);
        Result.Sort([](const auto& A, const auto& B){ return A.Id.LexicalLess(B.Id); });
        return Result;
    }

    const FPGItemOptionDefinition* Pick(const TArray<const FPGItemOptionDefinition*>& Pool, FRandomStream& Random, bool GreaterOnly)
    {
        double Total = 0.;
        for (const auto* Option : Pool) if (!GreaterOnly || Option->bAllowGreater) Total += Option->Weight;
        double Choice = Random.FRand() * Total;
        const FPGItemOptionDefinition* Last = nullptr;
        for (const auto* Option : Pool)
        {
            if (GreaterOnly && !Option->bAllowGreater) continue;
            Last = Option;
            Choice -= Option->Weight;
            if (Choice < 0.) return Option;
        }
        return Last;
    }
}

void PGItemGeneration::Range(const FPGItemDataRow& Def, const FPGItemOptionDefinition& Option, int32 Power, int32& Min, int32& Max)
{
    const double Alpha = Def.MaxPower > Def.MinPower ?
        FMath::Clamp((double(Power) - Def.MinPower) / (Def.MaxPower - Def.MinPower), 0., 1.) : 0.;
    Min = FMath::Min(Option.Cap, FMath::RoundToInt32(FMath::Lerp(double(Option.MinAtLowPower), double(Option.MinAtHighPower), Alpha)));
    Max = FMath::Min(Option.Cap, FMath::RoundToInt32(FMath::Lerp(double(Option.MaxAtLowPower), double(Option.MaxAtHighPower), Alpha)));
}

bool PGItemGeneration::Validate(const FPGItemDataRow& Def, const FPGEquipmentRarityRule& Rule, FString& Error)
{
    Error.Reset();
    auto Fail = [&](const TCHAR* Reason) { Error = FString::Printf(TEXT("장비 %d: %s"), Def.Id, Reason); return false; };
    if (!Def.bUseOptionPool)
    {
        if (Def.Id <= 0 || Def.Power < 1 || Def.Power > 100000 || Def.RollBonus < 0 || Def.RollBonus > 10000 || Def.BaseOptions.IsEmpty())
            return Fail(TEXT("기존 장비 기본값/범위 오류"));
        for (const auto& Pair : Def.BaseOptions)
            if (Pair.Key <= EPGStatType::None || Pair.Key >= EPGStatType::Max || Pair.Value < 0 || Pair.Value > 100000)
                return Fail(TEXT("기존 장비 능력치 오류"));
        return true;
    }
    if (Def.Id <= 0 || Def.Slot >= EPGEquipmentSlot::Max || Def.Rarity > EPGItemRarity::MythicUnique ||
        Rule.Rarity != Def.Rarity || Rule.OptionCount < 0 || Rule.OptionCount > 4 ||
        Def.MinPower < 1 || Def.MaxPower < Def.MinPower || Def.MaxPower > 100000 ||
        Def.DropQuality > EPGItemQuality::Ancestral || Def.BaseStatRanges.IsEmpty() || Def.ImplicitOptions.Num() > 1 ||
        !FMath::IsFinite(Def.GreaterMultiplier) || Def.GreaterMultiplier < 1.f || Def.GreaterMultiplier > 10.f)
        return Fail(TEXT("생성 범위/기본 성능/고정 특성 오류"));
    const bool Ancestral = Def.DropQuality == EPGItemQuality::Ancestral;
    if (Ancestral && (!Rule.bAllowAncestral || Def.Rarity < EPGItemRarity::Epic)) return Fail(TEXT("선조 품질을 허용하지 않는 등급"));
    TSet<FName> Ids;
    TMap<EPGStatType, int64> MaximumTotals;
    for (const auto* List : {&Def.BaseStatRanges, &Def.ImplicitOptions, &Def.OptionPool})
    {
        TSet<EPGStatType> Stats;
        for (const auto& Option : *List)
        {
            if (!Option.bEnabled) continue;
            if (Option.Id.IsNone() || Ids.Contains(Option.Id) || Option.DisplayName.IsEmpty() ||
                Option.Stat <= EPGStatType::None || Option.Stat >= EPGStatType::Max || Stats.Contains(Option.Stat) ||
                Option.Cap < 0 || Option.Cap > 100000 || Option.MinAtLowPower < 0 || Option.MinAtHighPower < 0 ||
                Option.MaxAtLowPower < Option.MinAtLowPower || Option.MaxAtHighPower < Option.MinAtHighPower ||
                Option.MaxAtLowPower > Option.Cap || Option.MaxAtHighPower > Option.Cap ||
                !FMath::IsFinite(Option.Weight) || Option.Weight < 0.f || Option.AllowedSlots.IsEmpty())
                return Fail(TEXT("옵션 ID/능력치/범위/가중치 오류 또는 중복"));
            for (auto Slot : Option.AllowedSlots) if (Slot >= EPGEquipmentSlot::Max) return Fail(TEXT("옵션 허용 부위 오류"));
            if (List != &Def.OptionPool && !AllowsSlot(Option, Def.Slot)) return Fail(TEXT("기본 성능/고정 특성의 부위 불일치"));
            Ids.Add(Option.Id); Stats.Add(Option.Stat);
            const int32 Max = FMath::Max(Option.MaxAtLowPower, Option.MaxAtHighPower);
            MaximumTotals.FindOrAdd(Option.Stat) += Ancestral && List == &Def.OptionPool && Option.bAllowGreater ?
                FMath::Min(Option.Cap, FMath::RoundToInt(Max * Def.GreaterMultiplier)) : Max;
        }
    }
    for (const auto& Pair : MaximumTotals) if (Pair.Value > 110000) return Fail(TEXT("동일 능력치의 합계가 저장 상한을 초과"));
    if (!Def.BaseStatRanges.ContainsByPredicate([](const auto& O){ return O.bEnabled; })) return Fail(TEXT("활성 기본 성능 없음"));
    const auto Pool = Candidates(Def);
    TSet<FName> Groups;
    int32 Capacity = 0, Greater = 0;
    for (const auto* Option : Pool)
    {
        if (Option->ExclusiveGroup.IsNone() || !Groups.Contains(Option->ExclusiveGroup)) ++Capacity;
        if (!Option->ExclusiveGroup.IsNone()) Groups.Add(Option->ExclusiveGroup);
        if (Option->bAllowGreater) ++Greater;
    }
    if (Capacity < Rule.OptionCount || (Rule.bFixedOptions && Pool.Num() != Rule.OptionCount))
        return Fail(TEXT("중복/배타를 제외한 적격 옵션 부족 또는 지정 옵션 수 불일치"));
    if (Ancestral)
    {
        if (Rule.OptionCount == 0 || Greater == 0 || Def.GreaterCountWeights.IsEmpty() || Def.GreaterCountWeights.Num() > 4 ||
            Def.GreaterCountWeights[0] <= 0.f) return Fail(TEXT("선조 상위 옵션 보장 불가"));
        for (float Weight : Def.GreaterCountWeights)
            if (!FMath::IsFinite(Weight) || Weight < 0.f) return Fail(TEXT("상위 옵션 수 가중치 오류"));
    }
    return true;
}

bool PGItemGeneration::Generate(const FPGItemDataRow& Def, const FPGEquipmentRarityRule& Rule,
    FRandomStream& Random, FPGItemInstance& Out, FString& Error)
{
    Out = FPGItemInstance();
    if (!Def.bUseOptionPool) { Error = TEXT("옵션 풀 생성이 비활성 상태입니다"); return false; }
    if (!Validate(Def, Rule, Error)) return false;
    FPGItemInstance Item;
    Item.Guid = FGuid::NewGuid(); Item.DefinitionId = Def.Id; Item.ItemVersion = 1;
    Item.Rarity = Def.Rarity; Item.OriginalRarity = Def.Rarity; Item.Quality = Def.DropQuality;
    Item.Power = Random.RandRange(Def.MinPower, Def.MaxPower);
    auto Append = [&](const FPGItemOptionDefinition& Option, EPGItemOptionSource Source)
    {
        FPGItemOptionRoll Roll; Roll.Id = Option.Id; Roll.Stat = Option.Stat; Roll.Source = Source;
        Range(Def, Option, Item.Power, Roll.Min, Roll.Max);
        Roll.Value = Random.RandRange(Roll.Min, Roll.Max);
        Item.OptionRolls.Add(Roll);
    };
    for (const auto* List : {&Def.BaseStatRanges, &Def.ImplicitOptions})
    {
        auto Sorted = *List;
        Sorted.Sort([](const auto& A, const auto& B){ return A.Id.LexicalLess(B.Id); });
        for (const auto& Option : Sorted) if (Option.bEnabled)
            Append(Option, List == &Def.BaseStatRanges ? EPGItemOptionSource::Base : EPGItemOptionSource::Implicit);
    }
    auto Pool = Candidates(Def);
    TArray<const FPGItemOptionDefinition*> Selected;
    for (int32 Index = 0; Index < Rule.OptionCount; ++Index)
    {
        // Reserve one eligible affix before exclusions can remove it.
        const auto* Option = Rule.bFixedOptions ? Pool[0] : Pick(Pool, Random, Item.Quality == EPGItemQuality::Ancestral && Index == 0);
        if (!Option) return false;
        Selected.Add(Option);
        Pool.RemoveAll([Option](const auto* Other){ return Other->Stat == Option->Stat ||
            (!Option->ExclusiveGroup.IsNone() && Other->ExclusiveGroup == Option->ExclusiveGroup); });
    }
    Selected.Sort([](const auto& A, const auto& B){ return A.Id.LexicalLess(B.Id); });
    TArray<int32> Eligible;
    for (const auto* Option : Selected)
    {
        Append(*Option, EPGItemOptionSource::Affix);
        if (Option->bAllowGreater) Eligible.Add(Item.OptionRolls.Num() - 1);
    }
    if (Item.Quality == EPGItemQuality::Ancestral)
    {
        const int32 Limit = FMath::Min(Eligible.Num(), Def.GreaterCountWeights.Num());
        double Total = 0.;
        for (int32 I = 0; I < Limit; ++I) Total += Def.GreaterCountWeights[I];
        double Choice = Random.FRand() * Total;
        int32 Count = 1;
        for (int32 I = 0; I < Limit; ++I) { Choice -= Def.GreaterCountWeights[I]; if (Choice < 0.) { Count = I + 1; break; } }
        for (int32 I = 0; I < Count; ++I)
        {
            const int32 Picked = Random.RandRange(0, Eligible.Num() - 1);
            auto& Roll = Item.OptionRolls[Eligible[Picked]];
            const auto* Option = Selected.FindByPredicate([&](const auto* O){ return O->Id == Roll.Id; });
            Roll.bGreater = true; Roll.Value = FMath::Min((*Option)->Cap, FMath::RoundToInt(Roll.Max * Def.GreaterMultiplier));
            Item.GreaterOptions.Add(Roll.Stat); Eligible.RemoveAt(Picked);
        }
    }
    for (const auto& Roll : Item.OptionRolls) Item.Options.FindOrAdd(Roll.Stat) += Roll.Value;
    if (!PGEquipmentRules::ValidateInstance(Item)) { Error = TEXT("생성 결과의 저장 범위 오류"); return false; }
    Out = MoveTemp(Item);
    return true;
}
