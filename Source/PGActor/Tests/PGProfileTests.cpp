#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGProfileTest, "PG.Progression.TransactionsAndSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGProfileTest::RunTest(const FString& Parameters)
{
    auto* GI = NewObject<UGameInstance>();
    auto* System = NewObject<UPGProfileSubsystem>(GI);
    System->Profile = NewObject<UPGProfileSave>(System);
    System->TestSlotPrefix = TEXT("PGAutomation_") + FGuid::NewGuid().ToString() + TEXT("_");
    System->Catalog = NewObject<UPGProgressionData>(System);
    System->Catalog->BagCapacity = 1;
    System->Catalog->DropChance = 1;
    FPGBuildDefinition Build; Build.Id = TEXT("Test"); System->Catalog->Builds.Add(Build);
    FPGItemDataRow Def; Def.Id = 1; Def.BaseOptions.Add(EPGStatType::Attack, 10); Def.RollBonus = 5;
    System->Catalog->Items.Add(Def);
    FRandomStream A(1234), B(1234);
    FPGItemInstance First, Second;
    TestTrue(TEXT("Drop generated"), System->RollDrop(A, First));
    System->RollDrop(B, Second);
    TestEqual(TEXT("Seed reproduces definition"), First.DefinitionId, Second.DefinitionId);
    TestEqual(TEXT("Seed reproduces options"), First.Options.FindRef(EPGStatType::Attack), Second.Options.FindRef(EPGStatType::Attack));
    TestTrue(TEXT("Unique identities"), First.Guid != Second.Guid);
    System->bInjectSaveFailure = true;
    TestFalse(TEXT("Failed save rejects pickup"), System->TryPickup(First));
    TestEqual(TEXT("Failed save leaves inventory intact"), System->Profile->Items.Num(), 0);
    System->Profile->Items.Add(First);
    TestFalse(TEXT("Duplicate pickup rejected"), System->TryPickup(First));
    TestFalse(TEXT("Full bag rejected"), System->TryPickup(Second));
    TestFalse(TEXT("Failed equip rejected"), System->Equip(First.Guid));
    TestEqual(TEXT("No equipment on failed save"), System->Profile->Equipment.Num(), 0);
    System->Profile->Equipment.Add(EPGEquipmentSlot::Weapon, First.Guid);
    System->Profile->LastReward = FGuid::NewGuid();
    TestFalse(TEXT("Duplicate reward rejected"), System->CommitReward(System->Profile->LastReward, 2, EPGStatType::Attack, 20));
    TestFalse(TEXT("Failed reward rejects checkpoint"), System->CommitReward(FGuid::NewGuid(), 2, EPGStatType::Attack, 20));
    TestEqual(TEXT("Checkpoint rolled back"), System->Profile->Checkpoint, 1);
    TestFalse(TEXT("Failed perk save rejected"), System->CommitReward(FGuid::NewGuid(), 2, EPGStatType::Attack, 0, EPGCombatPerk::Counter, 65));
    TestEqual(TEXT("Failed perk save leaves modifiers intact"), System->Profile->CombatPerks.Num(), 0);
    System->Profile->CombatPerks.Add(EPGCombatPerk::LifeSteal, 8);
    TArray<uint8> Bytes;
    TestTrue(TEXT("Save serializes"), UGameplayStatics::SaveGameToMemory(System->Profile, Bytes));
    auto* Restored = Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    TestTrue(TEXT("Restored valid"), System->Validate(Restored));
    if (Restored)
    {
        TestEqual(TEXT("Combat perk survives serialization"), Restored->CombatPerks.FindRef(EPGCombatPerk::LifeSteal), 8);
        TestEqual(TEXT("GUID preserved"), Restored->Items[0].Guid, First.Guid);
        TestEqual(TEXT("Rolled stats preserved"), Restored->Items[0].Options.FindRef(EPGStatType::Attack), First.Options.FindRef(EPGStatType::Attack));
        TestEqual(TEXT("Equipment preserved"), Restored->Equipment.FindRef(EPGEquipmentSlot::Weapon), First.Guid);
        Restored->Items[0].DefinitionId = 999999;
        TestTrue(TEXT("Unknown ID kept in quarantine"), System->Validate(Restored));
        Restored->Version = 99; TestFalse(TEXT("Future version blocked"), System->Validate(Restored));
        Restored->Version = 1; const FPGItemInstance Duplicate = Restored->Items[0]; Restored->Items.Add(Duplicate); TestFalse(TEXT("Duplicate identities invalid"), System->Validate(Restored));
    }
    System->bInjectSaveFailure = false;
    TestTrue(TEXT("Perk reward commits"), System->CommitReward(FGuid::NewGuid(), 2, EPGStatType::Attack, 0, EPGCombatPerk::Counter, 65));
    System->LoadProfile();
    TestEqual(TEXT("Perk restored from disk"), System->Profile->CombatPerks.FindRef(EPGCombatPerk::Counter), 65);
    TestTrue(TEXT("New run commits"), System->BeginNewRun());
    TestEqual(TEXT("New run removes all combat perks"), System->Profile->CombatPerks.Num(), 0);
    System->Profile->Equipment.Reset();
    System->Catalog->bRoguelikeRuns = true;
    System->Catalog->StartingItems = {1};
    System->Profile->SelectedRewards.Add(15000,1);
    TestTrue(TEXT("Rogue defeat persisted"),System->EndRun(false,4));
    TestTrue(TEXT("Rogue ended flag"),System->Profile->bRunEnded);
    TestTrue(TEXT("Fresh rogue run"),System->BeginNewRun());
    TestEqual(TEXT("Rogue resets reward selections"),System->Profile->SelectedRewards.Num(),0);
    TestEqual(TEXT("Rogue preserves best record"),System->Profile->BestStage,4);
    TestEqual(TEXT("Starter replaces previous inventory"),System->Profile->Items.Num(),1);
    TestTrue(TEXT("Starter gets a fresh identity"),System->Profile->Items[0].Guid != First.Guid);
    System->Catalog->bRoguelikeRuns = false;
    System->Profile->Items = {First}; System->Profile->Equipment.Reset();
    for (int32 Cycle = 0; Cycle < 20; ++Cycle)
    {
        TestTrue(TEXT("Equipment save succeeds"), System->Equip(First.Guid));
        const int64 Revision = System->Profile->Revision;
        auto* Disk = Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromSlot(System->SlotName(System->ActiveSlot), 0));
        TestTrue(TEXT("On-disk snapshot valid"), System->Validate(Disk));
        if (Disk) TestEqual(TEXT("On-disk revision"), Disk->Revision, Revision);
        TestTrue(TEXT("Unequip save succeeds"), System->Unequip(EPGEquipmentSlot::Weapon));
        TestEqual(TEXT("No item loss over cycles"), System->Profile->Items.Num(), 1);
    }
    auto* Previous = Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromSlot(System->SlotName(1-System->ActiveSlot), 0));
    TestTrue(TEXT("Previous committed slot retained"), System->Validate(Previous));
    if (Previous) TestEqual(TEXT("Previous slot is exactly one commit behind"), Previous->Revision, System->Profile->Revision - 1);
    TestTrue(TEXT("Discard unequipped item"), System->Discard(First.Guid));
    TestEqual(TEXT("Discard persisted"), System->Profile->Items.Num(), 0);
    auto* Invalid = DuplicateObject<UPGProfileSave>(System->Profile, System);
    Invalid->Checkpoint = -1;
    UGameplayStatics::SaveGameToSlot(Invalid, System->SlotName(System->ActiveSlot), 0);
    System->LoadProfile();
    TestFalse(TEXT("Corrupt latest snapshot falls back automatically"), System->IsSaveBlocked());
    TestEqual(TEXT("Previous snapshot items recovered"), System->Profile->Items.Num(), 1);
    Invalid->Checkpoint = 1; Invalid->Version = 99;
    UGameplayStatics::SaveGameToSlot(Invalid, System->SlotName(1-System->ActiveSlot), 0);
    System->LoadProfile();
    TestTrue(TEXT("Unknown version is preserved read-only"), System->IsSaveBlocked());
    UGameplayStatics::DeleteGameInSlot(System->SlotName(0), 0);
    UGameplayStatics::DeleteGameInSlot(System->SlotName(1), 0);
    return true;
}
#endif
