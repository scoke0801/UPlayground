#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/DataAsset/Progression/PGItemGeneration.h"
#include "PGData/DataAsset/Progression/PGLootRules.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGEquipmentExpansionTest, "PG.Progression.AuthoredEquipmentExpansion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGEquipmentExpansionTest::RunTest(const FString&)
{
    auto* System = NewObject<UPGProfileSubsystem>(NewObject<UGameInstance>());
    System->Catalog = LoadObject<UPGProgressionData>(nullptr, TEXT("/Game/DataCenter/Progression/DA_PGProgression.DA_PGProgression"));
    if (!TestNotNull(TEXT("Catalog loads"), System->Catalog.Get())) return false;
    System->TestSlotPrefix = TEXT("PGExpansion_") + FGuid::NewGuid().ToString() + TEXT("_");
    System->Profile = NewObject<UPGProfileSave>(System);
    FString Error;
    // Worldless rule/save test; the rendered expansion probe exercises EquipToSlot and GAS in game.
    auto SaveEquipment = [&](FGuid Guid, EPGEquipmentSlot Slot)
    {
        auto* Next = DuplicateObject<UPGProfileSave>(System->Profile, System);
        if (!PGEquipmentRules::PlanEquip(*System->Catalog, Next->Items, Next->Equipment, Guid, Slot, Next->Equipment, Error)) return false;
        return System->Commit(Next);
    };
    for (int32 Id = 15020; Id <= 15040; ++Id)
    {
        const auto* Def = System->Catalog->FindItem(Id);
        if (!TestNotNull(TEXT("Expansion definition exists"), Def)) return false;
        TestNotNull(TEXT("Dedicated icon resolves"), Def->Icon.LoadSynchronous());
        const auto* Rule = System->Catalog->RarityRules.FindByPredicate([&](const auto& R){ return R.Rarity == Def->Rarity; });
        if (!TestNotNull(TEXT("Rarity rule"), Rule)) return false;
        FPGItemInstance Item;
        for (int32 Seed = 0; Seed < 128; ++Seed)
        {
            FRandomStream Random(Seed);
            if (!TestTrue(TEXT("Generated expansion item"), PGItemGeneration::Generate(*Def, *Rule, Random, Item, Error))) return false;
            TestTrue(TEXT("Roll validates"), PGEquipmentRules::ValidateInstance(Item));
        }
        TestTrue(TEXT("Pickup commits"), System->TryPickup(Item));
        TestTrue(TEXT("Planned equipment commits"), SaveEquipment(Item.Guid, Def->Slot));
    }
    FRandomStream Random(971);
    const auto* Ring = System->Catalog->FindItem(15040);
    const auto* Rule = System->Catalog->RarityRules.FindByPredicate([&](const auto& R){ return R.Rarity == Ring->Rarity; });
    FPGItemInstance OtherRing;
    TestTrue(TEXT("Second ring generated"), PGItemGeneration::Generate(*Ring, *Rule, Random, OtherRing, Error));
    TestTrue(TEXT("Second ring pickup"), System->TryPickup(OtherRing));
    TestTrue(TEXT("Right ring plan commits"), SaveEquipment(OtherRing.Guid, EPGEquipmentSlot::RingRight));
    const auto Equipment = System->Profile->Equipment;
    TestEqual(TEXT("Seven new categories occupy eight slots"), Equipment.Num(), 8);
    const auto Items = System->Profile->Items;
    System->LoadProfile();
    TestTrue(TEXT("All slots survive disk reload"), System->Profile->Equipment.OrderIndependentCompareEqual(Equipment));
    TestEqual(TEXT("All acquired items survive"), System->Profile->Items.Num(), Items.Num());
    for (const auto& Item : Items)
    {
        const auto* Restored = System->Profile->Items.FindByPredicate([&](const auto& I){ return I.Guid == Item.Guid; });
        TestTrue(TEXT("Rolls preserved after reload"), Restored && Restored->Options.OrderIndependentCompareEqual(Item.Options));
    }
    for (const auto& Pool : System->Catalog->DropPools)
    {
        TSet<int32> Seen;
        for (int32 Sample = 0; Sample < 16000; ++Sample)
        {
            FPGItemInstance Drop;
            if (PGLootRules::Roll(*System->Catalog, Pool.Id, Random, Drop)) Seen.Add(Drop.DefinitionId);
        }
        for (int32 Id = 15020; Id <= 15040; ++Id)
        {
            const bool Allowed = Pool.Id != TEXT("Rogue.Boss") || (Id - 15020) % 3 != 0;
            TestEqual(TEXT("Actual drop pool reaches every allowed expansion item"), Seen.Contains(Id), Allowed);
        }
    }
    UGameplayStatics::DeleteGameInSlot(System->SlotName(0), 0);
    UGameplayStatics::DeleteGameInSlot(System->SlotName(1), 0);
    return true;
}
#endif
