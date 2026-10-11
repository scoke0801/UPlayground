#include "PGCraftingRules.h"
#include "PGProgressionData.h"

TArray<FPGTemperRecipe> PGCraftingRules::DefaultTempers()
{
    TArray<FPGTemperRecipe> Result;
    auto Add = [&](FName Id, const TCHAR* Name, EPGStatType Stat, TSet<EPGEquipmentSlot> Slots, int32 Value)
    {
        FPGTemperRecipe Recipe; Recipe.Id=Id; Recipe.DisplayName=FText::FromString(Name);
        Recipe.Stat=Stat; Recipe.Slots=MoveTemp(Slots); Recipe.BaseValue=Value;
        Recipe.Cost.Add(TEXT("Shard"),10); Result.Add(Recipe);
    };
    Add(TEXT("TemperAttack"),TEXT("공격력 보완"),EPGStatType::Attack,{EPGEquipmentSlot::Weapon,EPGEquipmentSlot::Hands,EPGEquipmentSlot::Accessory,EPGEquipmentSlot::RingLeft,EPGEquipmentSlot::RingRight},2);
    Add(TEXT("TemperDefense"),TEXT("보강"),EPGStatType::Defense,{EPGEquipmentSlot::Head,EPGEquipmentSlot::Chest,EPGEquipmentSlot::Hands,EPGEquipmentSlot::Legs,EPGEquipmentSlot::Feet,EPGEquipmentSlot::OffHand},3);
    TSet<EPGEquipmentSlot> AllSlots;
    for (uint8 Index=0; Index<uint8(EPGEquipmentSlot::Max); ++Index) AllSlots.Add(EPGEquipmentSlot(Index));
    Add(TEXT("TemperHealth"),TEXT("생명력 보완"),EPGStatType::Health,MoveTemp(AllSlots),20);
    return Result;
}

TArray<FPGMasterworkStep> PGCraftingRules::DefaultMasterworks()
{
    TArray<FPGMasterworkStep> Result;
    for (uint8 Slot=0; Slot<uint8(EPGEquipmentSlot::Max); ++Slot)
        for (uint8 Rarity=uint8(EPGItemRarity::Rare); Rarity<=uint8(EPGItemRarity::MythicUnique); ++Rarity)
            for (int32 Level=1; Level<=(Rarity==uint8(EPGItemRarity::Rare) ? 3 : 5); ++Level)
            {
                FPGMasterworkStep Step; Step.Slot=EPGEquipmentSlot(Slot); Step.Rarity=EPGItemRarity(Rarity); Step.Level=Level;
                Step.Cost.Add(TEXT("Shard"),15*Level);
                if (Level>=4) Step.Cost.Add(TEXT("Essence"),1);
                Result.Add(Step);
            }
    return Result;
}

bool PGCraftingRules::Validate(const UPGProgressionData& Catalog, FString& Reason)
{
    Reason=TEXT("장비 제작 설정을 확인하세요");
    auto ValidCost=[&](const TMap<FName,int64>& Cost)
    {
        if (Cost.IsEmpty()) return false;
        for (const auto& Pair : Cost)
            if (Pair.Value<=0 || !Catalog.SalvageRules.MaterialNames.Contains(Pair.Key)) return false;
        return true;
    };
    TSet<FName> Ids;
    for (const auto& Recipe : Catalog.TemperRecipes)
    {
        if (Recipe.Id.IsNone() || Ids.Contains(Recipe.Id) || Recipe.DisplayName.IsEmpty() || Recipe.Slots.IsEmpty() ||
            Recipe.Stat<=EPGStatType::None || Recipe.Stat>=EPGStatType::Max || Recipe.BaseValue<=0 ||
            Recipe.PowerStep<=0 || Recipe.ValuePerStep<0 || Recipe.MaxValue<Recipe.BaseValue || Recipe.MaxValue>100000 || !ValidCost(Recipe.Cost)) return false;
        for (const auto Slot : Recipe.Slots) if (Slot>=EPGEquipmentSlot::Max) return false;
        Ids.Add(Recipe.Id);
    }
    TSet<int32> Keys;
    for (const auto& Step : Catalog.MasterworkSteps)
    {
        const int32 Key=int32(Step.Slot)*100+int32(Step.Rarity)*10+Step.Level;
        if (Step.Slot>=EPGEquipmentSlot::Max || Step.Rarity<EPGItemRarity::Rare || Step.Rarity>EPGItemRarity::MythicUnique ||
            Step.Level<1 || Step.Level>(Step.Rarity==EPGItemRarity::Rare ? 3 : 5) || Step.BasePercent<=0 || Step.BasePercent>100 ||
            Keys.Contains(Key) || !ValidCost(Step.Cost)) return false;
        Keys.Add(Key);
    }
    Reason.Reset(); return true;
}

bool PGCraftingRules::Preview(const UPGProgressionData& Catalog, const FPGItemInstance& Item, FName RecipeId,
    FPGItemInstance& Result, TMap<FName,int64>& Cost, FString& Reason)
{
    Result=FPGItemInstance(); Cost.Reset();
    if (!Validate(Catalog,Reason)) return false;
    Reason=TEXT("이 장비는 강화할 수 없습니다");
    const auto* Def=Catalog.FindItem(Item.DefinitionId);
    const auto* Rarity=Catalog.RarityRules.FindByPredicate([&](const auto& Rule){return Rule.Rarity==Item.Rarity;});
    if (!Def || !Rarity || Item.ItemVersion!=1 || !PGEquipmentRules::ValidateInstance(Item)) return false;
    if (Item.bFavorite || Item.bJunk) { Reason=TEXT("즐겨찾기·잡동사니 표시를 해제한 뒤 강화하세요"); return false; }
    auto Next=Item;
    TMap<FName,int64> Required;
    if (!RecipeId.IsNone())
    {
        const auto* Recipe=Catalog.TemperRecipes.FindByPredicate([RecipeId](const auto& R){return R.Id==RecipeId;});
        if (!Rarity->bAllowTemper || Item.Rarity<EPGItemRarity::Rare || Item.Rarity>EPGItemRarity::Legendary || !Recipe) return false;
        if (!Recipe->Slots.Contains(Def->Slot)) { Reason=TEXT("이 부위에 사용할 수 없는 담금질입니다"); return false; }
        if (!Recipe->bAllowAffixDuplicate && Item.OptionRolls.ContainsByPredicate([&](const auto& Roll)
            {return Roll.Source==EPGItemOptionSource::Affix && Roll.Stat==Recipe->Stat;}))
        { Reason=TEXT("기존 일반 옵션과 중복되는 제작법입니다"); return false; }
        if (!Recipe->bAllowAffixDuplicate && Item.OptionRolls.IsEmpty() && Item.Options.Contains(Recipe->Stat))
        { Reason=TEXT("기존 옵션 출처를 확인할 수 없는 장비입니다"); return false; }
        const int32 Value=int32(FMath::Min<int64>(Recipe->MaxValue,int64(Recipe->BaseValue)+int64((Item.Power-1)/Recipe->PowerStep)*Recipe->ValuePerStep));
        if (Item.Temper.Id==RecipeId && Item.Temper.Stat==Recipe->Stat && Item.Temper.Value==Value)
        { Reason=TEXT("이미 같은 담금질이 적용되어 있습니다"); return false; }
        if (!Next.Temper.Id.IsNone())
        {
            auto& Old=Next.Options.FindOrAdd(Next.Temper.Stat); Old-=Next.Temper.Value;
            if (!Old && !Next.OptionRolls.ContainsByPredicate([&](const auto& R){return R.Stat==Next.Temper.Stat;})) Next.Options.Remove(Next.Temper.Stat);
        }
        Next.Temper=FPGItemOptionRoll(); Next.Temper.Id=RecipeId; Next.Temper.Stat=Recipe->Stat;
        Next.Temper.Value=Next.Temper.Min=Next.Temper.Max=Value;
        if (Next.Options.FindRef(Recipe->Stat)>110000-Value) { Reason=TEXT("장비 능력치 한도를 초과합니다"); return false; }
        Next.Options.FindOrAdd(Recipe->Stat)+=Value; Required=Recipe->Cost;
    }
    else
    {
        if (Item.MasterworkLevel>=Rarity->MaxMasterwork) { Reason=TEXT("명품화 최대 단계입니다"); return false; }
        const auto* Step=Catalog.MasterworkSteps.FindByPredicate([&](const auto& S)
            {return S.Slot==Def->Slot && S.Rarity==Item.Rarity && S.Level==Item.MasterworkLevel+1;});
        if (!Step) return false;
        TMap<EPGStatType,int32> Base;
        for (const auto& Roll : Item.OptionRolls) if (Roll.Source==EPGItemOptionSource::Base) Base.FindOrAdd(Roll.Stat)+=Roll.Value;
        bool Changed=false;
        for (const auto& Pair : Base)
        {
            if (Pair.Value<=0) continue;
            const int32 Increase=FMath::Max(1,FMath::RoundToInt32(double(Pair.Value)*Step->BasePercent/100.0));
            if (Next.Options.FindRef(Pair.Key)>110000-Increase) { Reason=TEXT("장비 능력치 한도를 초과합니다"); return false; }
            Next.Options.FindOrAdd(Pair.Key)+=Increase; Next.MasterworkBonuses.FindOrAdd(Pair.Key)+=Increase; Changed=true;
        }
        if (!Changed) { Reason=TEXT("강화할 기본 성능 기록이 없는 장비입니다"); return false; }
        ++Next.MasterworkLevel; Required=Step->Cost;
    }
    if (!PGEquipmentRules::ValidateInstance(Next)) { Reason=TEXT("강화 결과를 검증하지 못했습니다"); return false; }
    Result=MoveTemp(Next); Cost=MoveTemp(Required); Reason.Reset(); return true;
}
