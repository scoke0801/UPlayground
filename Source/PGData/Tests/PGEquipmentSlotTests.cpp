#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGEquipmentSlotTest, "PG.Progression.EquipmentSlots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGEquipmentSlotTest::RunTest(const FString&)
{
    auto* Catalog = NewObject<UPGProgressionData>(); Catalog->BagCapacity = 1;
    FPGItemDataRow Sword; Sword.Id=1;
    FPGItemDataRow Shield; Shield.Id=2; Shield.Slot=EPGEquipmentSlot::OffHand; Shield.EquipmentType=EPGEquipmentType::Shield;
    FPGItemDataRow TwoHand=Sword; TwoHand.Id=3; TwoHand.EquipmentType=EPGEquipmentType::TwoHandSword;
    FPGItemDataRow Ring; Ring.Id=4; Ring.Slot=EPGEquipmentSlot::RingLeft;
    Catalog->Items={Sword,Shield,TwoHand,Ring};
    TArray<FPGItemInstance> Items;
    for (int32 Id : {1,2,3,4,4}) { FPGItemInstance I; I.Guid=FGuid::NewGuid(); I.DefinitionId=Id; Items.Add(I); }
    TMap<EPGEquipmentSlot,FGuid> Equipped, Result; FString Error;
    Equipped.Add(EPGEquipmentSlot::Weapon,Items[0].Guid); Equipped.Add(EPGEquipmentSlot::OffHand,Items[1].Guid);
    Equipped.Add(EPGEquipmentSlot::RingLeft,Items[3].Guid); Equipped.Add(EPGEquipmentSlot::RingRight,Items[4].Guid);
    TestEqual(TEXT("Equipped items consume no bag space"), PGEquipmentRules::BagCount(Items,Equipped),1);
    auto Plan=[&](int32 Index, EPGEquipmentSlot Slot){return PGEquipmentRules::PlanEquip(*Catalog,Items,Equipped,Items[Index].Guid,Slot,Result,Error);};
    TestFalse(TEXT("Unsupported two hand stays locked"),Plan(2,EPGEquipmentSlot::Weapon));
    Catalog->Items[2].bEnableAdvancedWeapon=true;
    TestFalse(TEXT("Two returns need one extra cell"),Plan(2,EPGEquipmentSlot::Weapon));
    TestTrue(TEXT("Failure preserves equipment"),Result.OrderIndependentCompareEqual(Equipped));
    Catalog->BagCapacity=2;
    TestTrue(TEXT("Two hand atomic replacement"),Plan(2,EPGEquipmentSlot::Weapon));
    TestFalse(TEXT("Offhand released"),Result.Contains(EPGEquipmentSlot::OffHand));
    Equipped=Result;
    TestFalse(TEXT("Shield blocked by two hand"),Plan(1,EPGEquipmentSlot::OffHand));
    TestTrue(TEXT("One hand restores no old offhand"),Plan(0,EPGEquipmentSlot::Weapon));
    TestFalse(TEXT("Offhand remains empty"),Result.Contains(EPGEquipmentSlot::OffHand));
    Equipped=Result;
    TestFalse(TEXT("Wrong slot rejected"),Plan(1,EPGEquipmentSlot::Head));
    Catalog->BagCapacity=3;
    TestTrue(TEXT("Ring target explicit"),Plan(3,EPGEquipmentSlot::RingRight));
    TestFalse(TEXT("Same GUID not duplicated"),Result.Contains(EPGEquipmentSlot::RingLeft));
    Items[3].ItemVersion=1; Items[3].Rarity=EPGItemRarity::Unique;
    TestFalse(TEXT("Duplicate unique definition rejected"),Plan(3,EPGEquipmentSlot::RingLeft));
    return true;
}
#endif
