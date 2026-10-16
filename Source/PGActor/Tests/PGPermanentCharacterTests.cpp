#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGPermanentCharacterTest, "PG.Progression.PermanentCharacters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGPermanentCharacterTest::RunTest(const FString&)
{
    auto* System = NewObject<UPGProfileSubsystem>(NewObject<UGameInstance>());
    System->Profile = NewObject<UPGProfileSave>(System);
    System->Catalog = NewObject<UPGProgressionData>(System);
    System->TestSlotPrefix = TEXT("PGPermanent_") + FGuid::NewGuid().ToString() + TEXT("_");
    auto* Catalog = System->Catalog.Get();
    Catalog->bRoguelikeRuns = true; Catalog->BagCapacity = 1;
    FPGBuildDefinition Build; Build.Id = TEXT("Test"); Catalog->Builds.Add(Build);
    FPGItemDataRow Def; Def.Id = 1; Def.BaseOptions.Add(EPGStatType::Attack, 10);
    Catalog->Items.Add(Def); Catalog->StartingItems = {1};
    System->Profile->CharacterId = TEXT("A");
    System->bInjectSaveFailure = true;
    TestFalse(TEXT("First creation failure is atomic"), System->BeginNewRun(42));
    TestFalse(TEXT("Failed starter grant stays uninitialized"), System->Profile->bCharacterInitialized);
    System->bInjectSaveFailure = false;
    TestTrue(TEXT("First creation"), System->BeginNewRun(42));
    TestEqual(TEXT("One starter granted"), System->Profile->Items.Num(), 1);
    if (System->Profile->Items.IsEmpty()) return false;
    const auto Starter = System->Profile->Items[0];
    const FGuid OldRun = System->Profile->RunId;
    System->bInjectSaveFailure = true;
    TestFalse(TEXT("Kill persistence fails"), System->GrantExperience(OldRun, FGuid::NewGuid(), 650));
    TestFalse(TEXT("New dungeon fails without losing queued XP"), System->BeginNewRun(43));
    System->bInjectSaveFailure = false;
    TestTrue(TEXT("New dungeon flushes old pending XP into permanent growth"), System->BeginNewRun(43));
    TestEqual(TEXT("Pending XP retained"), System->Profile->Growth.Level, 4);
    TestEqual(TEXT("Pending XP remainder"), System->Profile->Growth.Experience, int64(50));
    TestEqual(TEXT("Starter identity not regenerated"), System->Profile->Items[0].Guid, Starter.Guid);
    TestTrue(TEXT("Old XP ledger cleared"), System->Profile->ExperienceClaims.IsEmpty());
    TestFalse(TEXT("Stale run XP rejected"), System->GrantExperience(OldRun, FGuid::NewGuid(), 50));

    auto* Draft = DuplicateObject<UPGProfileSave>(System->Profile, System);
    TestTrue(TEXT("Invest permanent points"), Catalog->LevelRules.Invest(Draft->Growth, {{EPGStatType::Attack, 2}}));
    Draft->Materials.Add(TEXT("Shard"), 77);
    Draft->Equipment.Reset(); Draft->Items[0].bFavorite = true;
    Draft->RewardBonuses.Add(EPGStatType::Attack, 99); Draft->CombatPerks.Add(EPGCombatPerk::LifeSteal, 8);
    Draft->SelectedRewards.Add(15001, 1); Draft->StageRewardCounts.Add(1, 1);
    TestTrue(TEXT("Growth fixture commits"), System->Commit(Draft));
    TestFalse(TEXT("Stash public API rejects unsafe world"), System->MoveToStash(Starter.Guid));
    System->bInjectSaveFailure = true;
    TestFalse(TEXT("Stash failure leaves source untouched"), System->MoveStashItem(Starter.Guid, false));
    TestEqual(TEXT("Failed stash keeps bag item"), System->Profile->Items.Num(), 1);
    System->bInjectSaveFailure = false;
    TestTrue(TEXT("Stash transfers ownership atomically"), System->MoveStashItem(Starter.Guid, false));
    TestEqual(TEXT("Bag released"), System->GetBagCount(), 0);
    TestFalse(TEXT("Stash duplicate transfer refused"), System->MoveStashItem(Starter.Guid, false));
    TestFalse(TEXT("Stashed drop cannot duplicate"), System->TryPickup(Starter));
    TestTrue(TEXT("New dungeon preserves empty bag and stash"), System->BeginNewRun(44));
    TestTrue(TEXT("No starter replenishment for empty bag"), System->Profile->Items.IsEmpty());
    TestEqual(TEXT("Stash count retained"), System->Profile->Stash.Num(), 1);
    TestTrue(TEXT("Card stats reset"), System->Profile->RewardBonuses.IsEmpty());
    TestTrue(TEXT("Card effects reset"), System->Profile->CombatPerks.IsEmpty());
    TestTrue(TEXT("Card selections reset"), System->Profile->SelectedRewards.IsEmpty());
    const auto Growth = System->Profile->Growth;
    TestEqual(TEXT("Investment retained"), Growth.Investments.FindRef(EPGStatType::Attack), 2);
    // Non-run startup can commit a profile before the first explicit dungeon entry.
    System->Profile->bCharacterInitialized = false;
    TestTrue(TEXT("Previously committed profile is not recreated"), System->BeginNewRun(45));
    TestTrue(TEXT("Committed growth survives an absent initialization marker"),
        FPGLevelState::StaticStruct()->CompareScriptStruct(&System->Profile->Growth, &Growth, 0));
    TestTrue(TEXT("Committed empty bag is not replenished"), System->Profile->Items.IsEmpty());

    System->bInjectSaveFailure = true;
    TestFalse(TEXT("Character switch failure"), System->ChangeCharacter(TEXT("B"), TEXT("A")));
    TestEqual(TEXT("Failed switch keeps identity"), System->Profile->CharacterId, FName(TEXT("A")));
    TestTrue(TEXT("Failed switch creates no archive"), System->Profile->OtherCharacters.IsEmpty());
    System->bInjectSaveFailure = false;
    TestTrue(TEXT("Character B creation"), System->ChangeCharacter(TEXT("B"), TEXT("A")));
    TestEqual(TEXT("B starts at initial level"), System->Profile->Growth.Level, Catalog->LevelRules.StartingLevel);
    TestTrue(TEXT("B has independent materials"), System->Profile->Materials.IsEmpty());
    TestTrue(TEXT("B has independent stash"), System->Profile->Stash.IsEmpty());
    TestEqual(TEXT("B gets one starter"), System->Profile->Items.Num(), 1);
    const FGuid BStarter = System->Profile->Items[0].Guid;
    TestNotEqual(TEXT("Starter ownership is independent"), BStarter, Starter.Guid);
    System->LoadProfile();
    TestTrue(TEXT("Archive survives reload"), System->Profile->OtherCharacters.Contains(TEXT("A")));
    TestTrue(TEXT("Return to A"), System->ChangeCharacter(TEXT("A"), TEXT("B")));
    TestTrue(TEXT("Exact growth restored"), FPGLevelState::StaticStruct()->CompareScriptStruct(&System->Profile->Growth, &Growth, 0));
    TestEqual(TEXT("A materials restored"), System->Profile->Materials.FindRef(TEXT("Shard")), int64(77));
    TestEqual(TEXT("A stash identity restored"), System->Profile->Stash[0].Guid, Starter.Guid);
    TestTrue(TEXT("Favorite protection retained"), System->Profile->Stash[0].bFavorite);
    TestTrue(TEXT("Withdraw from stash"), System->MoveStashItem(Starter.Guid, true));
    TestTrue(TEXT("Empty stash after withdrawal"), System->Profile->Stash.IsEmpty());
    auto Extra = Starter; Extra.Guid = FGuid::NewGuid();
    Draft = DuplicateObject<UPGProfileSave>(System->Profile, System); Draft->Stash.Add(Extra);
    TestTrue(TEXT("Second stash fixture"), System->Commit(Draft));
    TestFalse(TEXT("Full bag cannot withdraw"), System->MoveStashItem(Extra.Guid, true));
    TestEqual(TEXT("Full bag keeps stash intact"), System->Profile->Stash.Num(), 1);
    TestTrue(TEXT("Switch back to B"), System->ChangeCharacter(TEXT("B"), TEXT("A")));
    TestEqual(TEXT("B starter is never regenerated"), System->Profile->Items[0].Guid, BStarter);
    FString Reason;
    TestFalse(TEXT("Public character switch requires safe first-stage world"), System->CanChangeCharacter(Reason));

    // v8 bytes must survive a failed migration and the successful v9 migration.
    for (int32 I=0; I<2; ++I) UGameplayStatics::DeleteGameInSlot(System->SlotName(I), 0);
    auto* Legacy = NewObject<UPGProfileSave>(System);
    Legacy->Version = 8; Legacy->Revision = 10; Legacy->Growth = Growth;
    Legacy->Items = {Starter}; Legacy->Materials.Add(TEXT("Shard"), 123);
    Legacy->RunId = FGuid::NewGuid(); Legacy->RunSeed = 55;
    TestTrue(TEXT("Legacy v8 fixture"), UGameplayStatics::SaveGameToSlot(Legacy, System->SlotName(0), 0));
    TArray<uint8> Original, After;
    UGameplayStatics::LoadDataFromSlot(Original, System->SlotName(0), 0);
    System->bInjectSaveFailure = true; System->LoadProfile();
    TestTrue(TEXT("Migration failure blocks writes"), System->IsSaveBlocked());
    TestEqual(TEXT("Failed migration remains v8"), System->Profile->Version, 8);
    System->bInjectSaveFailure = false; System->LoadProfile();
    TestFalse(TEXT("Migration retry succeeds"), System->IsSaveBlocked());
    TestEqual(TEXT("Migrated to v9"), System->Profile->Version, 9);
    UGameplayStatics::LoadDataFromSlot(After, System->SlotName(0), 0);
    TestTrue(TEXT("Original slot remains byte-identical"), Original == After);
    TestTrue(TEXT("Legacy growth preserved"), FPGLevelState::StaticStruct()->CompareScriptStruct(&System->Profile->Growth, &Growth, 0));
    TestTrue(TEXT("Legacy unnamed character binds without resetting"), System->ChangeCharacter(TEXT("A"), TEXT("A")));
    TestTrue(TEXT("Migrated new dungeon"), System->BeginNewRun(56));
    TestEqual(TEXT("Legacy item retained without starter duplication"), System->Profile->Items.Num(), 1);
    TestEqual(TEXT("Legacy materials retained"), System->Profile->Materials.FindRef(TEXT("Shard")), int64(123));
    System->LoadProfile();
    TestEqual(TEXT("Migrated permanent growth survives reload"), System->Profile->Growth.Level, Growth.Level);
    TestTrue(TEXT("Migration keeps original ownership before switch"), System->ChangeCharacter(TEXT("B"), TEXT("A")));
    Draft = DuplicateObject<UPGProfileSave>(System->Profile, System);
    Draft->OtherCharacters[TEXT("A")].Items[0].DefinitionId = 999999;
    TestTrue(TEXT("Unsupported archived fixture writes"), UGameplayStatics::SaveGameToSlot(Draft, System->SlotName(System->ActiveSlot), 0));
    System->LoadProfile();
    TestTrue(TEXT("Missing archived item data blocks writes instead of losing the character"), System->IsSaveBlocked());
    for (int32 I=0; I<2; ++I) UGameplayStatics::DeleteGameInSlot(System->SlotName(I), 0);
    return true;
}
#endif
