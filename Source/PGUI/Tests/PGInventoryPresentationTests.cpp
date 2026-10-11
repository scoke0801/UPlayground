#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGUI/Widget/Window/PGInventoryPresentation.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGInventoryComparisonTest,"PG.UI.InventoryComparison",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGInventoryComparisonTest::RunTest(const FString& Parameters)
{
    TSet<FString> GradeNames;
    for (uint8 Index=0;Index<=uint8(EPGItemRarity::MythicUnique);++Index)
        GradeNames.Add(PGInventoryPresentation::RarityName(EPGItemRarity(Index)).ToString());
    TestEqual(TEXT("All seven grades have distinct text labels"),GradeNames.Num(),7);
    FPGItemInstance Current, Selected;
    Current.Options={{EPGStatType::Attack,100},{EPGStatType::Defense,35},{EPGStatType::CriticalRate,250}};
    Selected.Options={{EPGStatType::Attack,125},{EPGStatType::CriticalRate,375},{EPGStatType::Health,80}};
    const auto Rows=PGInventoryPresentation::Compare(Selected,&Current);
    TestEqual(TEXT("Union includes lost defense and new health"),Rows.Num(),4);
    const auto* Defense=Rows.FindByPredicate([](const auto& Row){return Row.Stat==EPGStatType::Defense;});
    TestTrue(TEXT("Removed defense remains visible as a loss"),Defense && Defense->Selected==0 && Defense->Delta()==-35);
    const auto* Crit=Rows.FindByPredicate([](const auto& Row){return Row.Stat==EPGStatType::CriticalRate;});
    TestTrue(TEXT("Crit delta is retained in authoritative basis points"),Crit && Crit->Delta()==125);
    TestEqual(TEXT("Crit chance displayed as percent"),PGInventoryPresentation::StatValue(EPGStatType::CriticalRate,375).ToString(),FString(TEXT("3.75%")));
    TestEqual(TEXT("Crit delta uses percentage points"),PGInventoryPresentation::StatValue(EPGStatType::CriticalRate,125,true).ToString(),FString(TEXT("+1.25%p")));
    TestEqual(TEXT("Negative crit delta keeps sign"),PGInventoryPresentation::StatValue(EPGStatType::CriticalRate,-125,true).ToString(),FString(TEXT("-1.25%p")));
    const auto EmptySlot=PGInventoryPresentation::Compare(Selected,nullptr);
    for (const auto& Row : EmptySlot) TestEqual(TEXT("Empty slot compares against zero"),Row.Current,0);
    const auto SameItem=PGInventoryPresentation::Compare(Selected,&Selected);
    for (const auto& Row : SameItem) TestEqual(TEXT("Selected equipped item has no gain"),Row.Delta(),0);
    for (int32 I=1;I<Rows.Num();++I) TestTrue(TEXT("Stable enum ordering"),uint8(Rows[I-1].Stat)<uint8(Rows[I].Stat));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGInventoryBagTest,"PG.UI.InventoryBag",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGInventoryBagTest::RunTest(const FString&)
{
    auto* Catalog=NewObject<UPGProgressionData>();
    FPGItemDataRow Sword; Sword.Id=1; Sword.Slot=EPGEquipmentSlot::Weapon;
    FPGItemDataRow Ring; Ring.Id=2; Ring.Slot=EPGEquipmentSlot::RingRight;
    Catalog->Items={Sword,Ring};
    TArray<FPGItemInstance> Items;
    for (int32 I=0;I<5;++I)
    {
        FPGItemInstance Item; Item.Guid=FGuid(0,0,0,I+1); Item.DefinitionId=I==2 ? 2 : 1;
        Item.ItemVersion=1; Item.Power=I==1 || I==3 ? 100 : 20; Item.Rarity=I==2 ? EPGItemRarity::Epic : EPGItemRarity::Rare;
        Item.Quality=I==2 ? EPGItemQuality::Ancestral : EPGItemQuality::Standard;
        Item.bFavorite=I==1; Item.bJunk=I==3; Items.Add(Item);
    }
    TMap<EPGEquipmentSlot,FGuid> Equipment={{EPGEquipmentSlot::Weapon,Items[4].Guid}};
    FPGInventoryFilter Filter;
    auto Result=PGInventoryPresentation::BagItems(Items,Equipment,*Catalog,Filter,EPGInventorySort::Recent);
    TestEqual(TEXT("Equipped excluded"),Result.Num(),4); TestEqual(TEXT("Newest bag item first"),Result[0],Items[3].Guid);
    Result=PGInventoryPresentation::BagItems(Items,Equipment,*Catalog,Filter,EPGInventorySort::Power);
    TestEqual(TEXT("Power tie uses acquisition order"),Result[0],Items[3].Guid);
    TestEqual(TEXT("Second equal power"),Result[1],Items[1].Guid);
    Result=PGInventoryPresentation::BagItems(Items,Equipment,*Catalog,Filter,EPGInventorySort::Rarity);
    TestEqual(TEXT("Saved grade sorts first"),Result[0],Items[2].Guid);
    Filter.Slot=int32(EPGEquipmentSlot::RingLeft); Filter.Rarity=int32(EPGItemRarity::Epic); Filter.Quality=1;
    Result=PGInventoryPresentation::BagItems(Items,Equipment,*Catalog,Filter,EPGInventorySort::Slot);
    TestEqual(TEXT("Combined ring grade quality filter"),Result.Num(),1);
    Filter=FPGInventoryFilter(); Filter.Mark=1;
    Result=PGInventoryPresentation::BagItems(Items,Equipment,*Catalog,Filter,EPGInventorySort::Recent);
    TestTrue(TEXT("Favorites only"),Result.Num()==1 && Result[0]==Items[1].Guid);
    Filter.Mark=2;
    Result=PGInventoryPresentation::BagItems(Items,Equipment,*Catalog,Filter,EPGInventorySort::Recent);
    TestTrue(TEXT("Junk only"),Result.Num()==1 && Result[0]==Items[3].Guid);
    TestEqual(TEXT("Ownership order unchanged"),Items[0].Guid,FGuid(0,0,0,1));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGInventoryPreviewTest,"PG.UI.InventoryReplacementPreview",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGInventoryPreviewTest::RunTest(const FString&)
{
    auto* Catalog=NewObject<UPGProgressionData>(); Catalog->BagCapacity=8;
    TArray<FPGItemInstance> Items;
    for (int32 I=0;I<6;++I)
    {
        FPGItemDataRow Def; Def.Id=I+1;
        Def.Slot=I==1 ? EPGEquipmentSlot::OffHand : I>=3 ? EPGEquipmentSlot::RingLeft : EPGEquipmentSlot::Weapon;
        Def.EquipmentType=I==1 ? EPGEquipmentType::Shield : I==2 ? EPGEquipmentType::TwoHandSword : EPGEquipmentType::Standard;
        Def.bEnableAdvancedWeapon=true; Catalog->Items.Add(Def);
        FPGItemInstance Item; Item.Guid=FGuid(0,0,0,I+1); Item.DefinitionId=Def.Id;
        Item.Options.Add(I==1 ? EPGStatType::Defense : EPGStatType::Attack,(I+1)*10); Items.Add(Item);
    }
    TMap<EPGEquipmentSlot,FGuid> Equipment={{EPGEquipmentSlot::Weapon,Items[0].Guid},
        {EPGEquipmentSlot::OffHand,Items[1].Guid},{EPGEquipmentSlot::RingLeft,Items[3].Guid},{EPGEquipmentSlot::RingRight,Items[4].Guid}};
    TArray<FPGInventoryStatComparison> Rows; FString Reason;
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* Owner=World->SpawnActor<AActor>();
    auto* ASC=NewObject<UPGAbilitySystemComponent>(Owner);
    Owner->AddInstanceComponent(ASC); ASC->RegisterComponent(); ASC->InitAbilityActorInfo(Owner,Owner);
    ASC->InitializeCombatStats({{EPGStatType::Attack,100},{EPGStatType::Defense,50}});
    ASC->SetEquipmentBonuses({{EPGStatType::Attack,15}});
    auto CheckGAS=[&](FGuid Guid,EPGEquipmentSlot Slot)
    {
        TMap<EPGStatType,int32> Before={{EPGStatType::Attack,7}}, After=Before;
        TMap<EPGEquipmentSlot,FGuid> Next;
        TestTrue(TEXT("Actual candidate accepted"),PGEquipmentRules::PlanEquip(*Catalog,Items,Equipment,Guid,Slot,Next,Reason));
        for (const auto& Item : Items)
            for (const auto& Pair : Item.Options)
            {
                if (Equipment.FindKey(Item.Guid)) Before.FindOrAdd(Pair.Key)+=Pair.Value;
                if (Next.FindKey(Item.Guid)) After.FindOrAdd(Pair.Key)+=Pair.Value;
            }
        ASC->SetProfileBonuses(Before);
        TMap<EPGStatType,float> Expected;
        for (const auto& Row : Rows)
            Expected.Add(Row.Stat,ASC->GetStatDetails(Row.Stat,float(Row.Selected+(Row.Stat==EPGStatType::Attack ? 7 : 0))).Expected);
        ASC->SetProfileBonuses(After);
        for (const auto& Pair : Expected)
            TestEqual(TEXT("Ring/two-hand final preview matches applied candidate"),ASC->GetCombatStat(Pair.Key),Pair.Value);
    };
    TestTrue(TEXT("Two hand candidate valid"),PGInventoryPresentation::PreviewEquip(Items,Equipment,*Catalog,Items[2].Guid,EPGEquipmentSlot::Weapon,Rows,Reason));
    const auto* Defense=Rows.FindByPredicate([](const auto& R){return R.Stat==EPGStatType::Defense;});
    TestTrue(TEXT("Two hand loses shield defense"),Defense && Defense->Delta()==-20);
    CheckGAS(Items[2].Guid,EPGEquipmentSlot::Weapon);
    for (const auto Target : {EPGEquipmentSlot::RingLeft,EPGEquipmentSlot::RingRight})
    {
        TestTrue(TEXT("Both ring targets preview"),PGInventoryPresentation::PreviewEquip(Items,Equipment,*Catalog,Items[5].Guid,Target,Rows,Reason));
        const auto* Attack=Rows.FindByPredicate([](const auto& R){return R.Stat==EPGStatType::Attack;});
        TestTrue(TEXT("Only selected ring is replaced"),Attack && Attack->Delta()==(Target==EPGEquipmentSlot::RingLeft ? 20 : 10));
        CheckGAS(Items[5].Guid,Target);
    }
    TestTrue(TEXT("Equipped right ring preview"),PGInventoryPresentation::PreviewEquip(Items,Equipment,*Catalog,Items[4].Guid,EPGEquipmentSlot::RingRight,Rows,Reason));
    for (const auto& Row : Rows) TestEqual(TEXT("Equipped item has zero delta"),Row.Delta(),0);
    Catalog->BagCapacity=2;
    TestFalse(TEXT("Two hand extra return must fit"),PGInventoryPresentation::PreviewEquip(Items,Equipment,*Catalog,Items[2].Guid,EPGEquipmentSlot::Weapon,Rows,Reason));
    TestTrue(TEXT("Failure clears preview and explains lock"),Rows.IsEmpty() && !Reason.IsEmpty());
    TestEqual(TEXT("Preview preserves equipment"),Equipment.Num(),4);
    World->DestroyWorld(false);
    return true;
}
#endif
