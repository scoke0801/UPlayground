#include "PGLegendaryRules.h"
#include "PGProgressionData.h"

TArray<FPGLegendaryEffect> PGLegendaryRules::Defaults()
{
    TArray<FPGLegendaryEffect> Result;
    auto Add = [&](const TCHAR* Id, const TCHAR* Name, const TCHAR* Description, EPGCombatPerk Perk, int32 Percent, TArray<EPGEquipmentSlot> Slots)
    {
        FPGLegendaryEffect Effect; Effect.Id=Id; Effect.DisplayName=FText::FromString(Name);
        Effect.Description=FText::FromString(Description); Effect.Perk=Perk; Effect.Percent=Percent;
        for (auto Slot : Slots) Effect.SlotMultipliers.Add(Slot,1.f);
        Effect.Cost.Add(TEXT("Shard"),20); Effect.Cost.Add(TEXT("Essence"),1); Result.Add(Effect);
    };
    Add(TEXT("Bloodletting"),TEXT("깊은 상처"),TEXT("직접 적중 시 출혈을 부여합니다."),EPGCombatPerk::Bleed,15,{EPGEquipmentSlot::Weapon,EPGEquipmentSlot::Hands});
    Add(TEXT("Impact"),TEXT("울리는 강타"),TEXT("강타 적중 시 주변에 충격파 피해를 줍니다."),EPGCombatPerk::Shockwave,20,{EPGEquipmentSlot::Weapon,EPGEquipmentSlot::Accessory});
    Add(TEXT("Siphon"),TEXT("생명 수확"),TEXT("가한 피해의 일부만큼 생명력을 회복합니다."),EPGCombatPerk::LifeSteal,3,{EPGEquipmentSlot::Weapon,EPGEquipmentSlot::RingLeft,EPGEquipmentSlot::RingRight});
    return Result;
}
int32 PGLegendaryRules::Strength(const FPGLegendaryEffect& Effect, EPGEquipmentSlot Slot)
{
    const float Scale=Effect.SlotMultipliers.FindRef(Slot);
    return FMath::IsFinite(Scale) && Scale>0 && Scale<=10 && Effect.Percent>0 && Effect.Percent<=100
        ? FMath::Clamp(FMath::RoundToInt(Effect.Percent*Scale),1,100) : 0;
}
bool PGLegendaryRules::Validate(const UPGProgressionData& Catalog, FString& Error)
{
    TSet<FName> Ids;
    for (const auto& Effect : Catalog.LegendaryEffects)
    {
        if (Effect.Id.IsNone() || Ids.Contains(Effect.Id) || Effect.DisplayName.IsEmpty() || Effect.Description.IsEmpty() ||
            Effect.Perk<=EPGCombatPerk::None || Effect.Perk>=EPGCombatPerk::Max || Effect.Percent<1 || Effect.Percent>100 || Effect.SlotMultipliers.IsEmpty() || Effect.Cost.IsEmpty())
        { Error=TEXT("전설 효과 정의 오류"); return false; }
        Ids.Add(Effect.Id);
        for (const auto& Pair : Effect.SlotMultipliers)
            if (Pair.Key>=EPGEquipmentSlot::Max || Strength(Effect,Pair.Key)==0) { Error=TEXT("전설 효과 부위 배율 오류"); return false; }
        if (Effect.SlotMultipliers.FindRef(EPGEquipmentSlot::RingLeft)!=Effect.SlotMultipliers.FindRef(EPGEquipmentSlot::RingRight))
        { Error=TEXT("반지 좌우 효과 배율은 같아야 합니다"); return false; }
        for (const auto& Pair : Effect.Cost)
            if (!Catalog.SalvageRules.MaterialNames.Contains(Pair.Key) || Pair.Value<=0) { Error=TEXT("각인 재료 비용 오류"); return false; }
    }
    for (const auto& Def : Catalog.Items)
        if (!Def.LegendaryEffect.IsNone())
        {
            const auto* Effect=Catalog.FindLegendary(Def.LegendaryEffect);
            if (Def.Rarity!=EPGItemRarity::Legendary || !Effect || !Strength(*Effect,Def.Slot) || !Def.CombatPerks.IsEmpty())
            { Error=TEXT("전설 장비 효과/부위 또는 중복 특성 오류"); return false; }
        }
    return true;
}
bool PGLegendaryRules::ValidateItem(const UPGProgressionData& Catalog, const FPGItemInstance& Item)
{
    if (Item.LegendaryEffect.IsNone()) return Item.LegendaryPercent==0;
    const auto* Effect=Catalog.FindLegendary(Item.LegendaryEffect);
    const auto* Def=Catalog.FindItem(Item.DefinitionId);
    return Effect && Def && Item.Rarity==EPGItemRarity::Legendary && Item.LegendaryPercent>0 && Item.LegendaryPercent<=100 && Strength(*Effect,Def->Slot)>0;
}
TMap<FName, FGuid> PGLegendaryRules::Active(const UPGProgressionData& Catalog, const TArray<FPGItemInstance>& Items, const TMap<EPGEquipmentSlot, FGuid>& Equipment)
{
    TMap<FName,FGuid> Winners; TMap<FName,int32> Values;
    // Fixed serialized slot order resolves ties, independent of map/inventory order.
    for (uint8 Index=0; Index<uint8(EPGEquipmentSlot::Max); ++Index)
    {
        const FGuid Guid=Equipment.FindRef(EPGEquipmentSlot(Index));
        const auto* Item=Items.FindByPredicate([Guid](const auto& I){return I.Guid==Guid;});
        if (!Item || Item->LegendaryEffect.IsNone() || !ValidateItem(Catalog,*Item)) continue;
        if (Item->LegendaryPercent>Values.FindRef(Item->LegendaryEffect))
        { Values.Add(Item->LegendaryEffect,Item->LegendaryPercent); Winners.Add(Item->LegendaryEffect,Guid); }
    }
    return Winners;
}
TMap<EPGCombatPerk,int32> PGLegendaryRules::Perks(const UPGProgressionData& Catalog, const TArray<FPGItemInstance>& Items, const TMap<EPGEquipmentSlot,FGuid>& Equipment)
{
    TMap<EPGCombatPerk,int32> Result;
    for (const auto& Pair : Active(Catalog,Items,Equipment))
    {
        const auto* Item=Items.FindByPredicate([&](const auto& I){return I.Guid==Pair.Value;});
        Result.FindOrAdd(Catalog.FindLegendary(Pair.Key)->Perk)+=Item->LegendaryPercent;
    }
    return Result;
}
