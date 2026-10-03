#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGShared/Shared/Structure/PGRunRandom.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGLootTransactionTest, "PG.Progression.LootTransactions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGLootTransactionTest::RunTest(const FString& Parameters)
{
    auto* GI = NewObject<UGameInstance>();
    auto* System = NewObject<UPGProfileSubsystem>(GI);
    System->Profile = NewObject<UPGProfileSave>(System);
    System->Catalog = NewObject<UPGProgressionData>(System);
    System->TestSlotPrefix = TEXT("PGLootTest_") + FGuid::NewGuid().ToString() + TEXT("_");
    System->Catalog->bRoguelikeRuns = true; System->Catalog->BagCapacity = 1;
    FPGItemDataRow Def; Def.Id = 1; Def.BaseOptions = {{EPGStatType::Attack, 15}};
    System->Catalog->Items.Add(Def);
    FPGBuildDefinition Build; Build.Id = TEXT("Test"); System->Catalog->Builds.Add(Build);
    System->Profile->RunSeed = 173001;
    System->bInjectSaveFailure = true;
    TestFalse(TEXT("Identity migration respects save failure"), System->EnsureRunSeed());
    TestFalse(TEXT("Failed migration does not expose an identity"), System->Profile->RunId.IsValid());
    System->bInjectSaveFailure = false;
    TestTrue(TEXT("Old seeded save receives identity"), System->EnsureRunSeed());
    TestEqual(TEXT("Migration preserves established run seed"), System->Profile->RunSeed, 173001);
    const FGuid RunId = System->Profile->RunId;
    FPGItemInstance Item; Item.DefinitionId = 1; Item.Options = Def.BaseOptions;
    Item.Guid = PGRunRandom::LootGuid(RunId, 2, 2, 15104, 0);
    System->bInjectSaveFailure = true;
    TestFalse(TEXT("Pickup failure retains unclaimed drop"), System->TryPickup(Item));
    TestFalse(TEXT("Failed pickup never claims identity"), System->HasClaimedLoot(Item.Guid));
    System->bInjectSaveFailure = false;
    TestTrue(TEXT("Pickup commits inventory and claim together"), System->TryPickup(Item));
    FPGItemInstance Boss = Item; Boss.Guid = PGRunRandom::LootGuid(RunId, 6, 0, 15106, 0);
    TestFalse(TEXT("Full bag cannot consume ground loot"), System->TryPickup(Boss));
    TestFalse(TEXT("Full bag leaves identity unclaimed"), System->HasClaimedLoot(Boss.Guid));
    TestTrue(TEXT("Discard persists"), System->Discard(Item.Guid));
    System->LoadProfile();
    TestEqual(TEXT("Run identity survives process-equivalent disk reload"), System->Profile->RunId, RunId);
    TestFalse(TEXT("Checkpoint replay cannot reclaim a discarded drop"), System->TryPickup(Item));
    FPGItemInstance Filler = Item; Filler.Guid = FGuid::NewGuid();
    TestTrue(TEXT("Fill bag before boss result"), System->TryPickup(Filler));
    TestTrue(TEXT("Boss reward queues despite full bag"), System->QueueBossReward(Boss));
    TestTrue(TEXT("Duplicate boss notification is idempotent"), System->QueueBossReward(Boss));
    FPGItemInstance OtherBoss = Boss; OtherBoss.Guid = FGuid::NewGuid();
    TestFalse(TEXT("A second item cannot replace queued result"), System->QueueBossReward(OtherBoss));
    System->bInjectSaveFailure = true;
    TestFalse(TEXT("Victory failure rejects reward and win together"), System->EndRun(true, 6));
    TestEqual(TEXT("Failed victory leaves win count"), System->Profile->CompletedRuns, 0);
    TestFalse(TEXT("Failed victory leaves result empty"), System->Profile->BossReward.Guid.IsValid());
    TestFalse(TEXT("Failed victory leaves reward unclaimed"), System->HasClaimedLoot(Boss.Guid));
    System->bInjectSaveFailure = false;
    TestTrue(TEXT("Victory retry commits queued reward"), System->EndRun(true, 6));
    TestTrue(TEXT("Duplicate victory is harmless"), System->EndRun(true, 6));
    TestEqual(TEXT("One victory recorded"), System->Profile->CompletedRuns, 1);
    TestEqual(TEXT("Result slot preserves rolled identity"), System->Profile->BossReward.Guid, Boss.Guid);
    TestEqual(TEXT("Boss reward does not exceed bag capacity"), System->Profile->Items.Num(), 1);
    System->LoadProfile();
    TestEqual(TEXT("Result reward survives disk reload"), System->Profile->BossReward.Guid, Boss.Guid);
    TestTrue(TEXT("Result reward claim survives disk reload"), System->HasClaimedLoot(Boss.Guid));
    System->bInjectSaveFailure = true;
    TestFalse(TEXT("New run failure preserves result"), System->BeginNewRun());
    TestEqual(TEXT("Result kept until explicit successful new run"), System->Profile->BossReward.Guid, Boss.Guid);
    System->bInjectSaveFailure = false;
    TestTrue(TEXT("Explicit new run succeeds"), System->BeginNewRun());
    TestFalse(TEXT("New run clears result slot"), System->Profile->BossReward.Guid.IsValid());
    TestEqual(TEXT("New run clears claim ledger"), System->Profile->ClaimedLoot.Num(), 0);
    TestNotEqual(TEXT("New run identity changes"), System->Profile->RunId, RunId);
    TestEqual(TEXT("Permanent victory remains"), System->Profile->CompletedRuns, 1);
    UGameplayStatics::DeleteGameInSlot(System->SlotName(0), 0);
    UGameplayStatics::DeleteGameInSlot(System->SlotName(1), 0);
    return true;
}
#endif
