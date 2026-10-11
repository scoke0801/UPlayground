#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGInvestmentRulesTest, "PG.Progression.InvestmentRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGInvestmentRulesTest::RunTest(const FString&)
{
    FPGLevelRules Rules;
    auto State=Rules.InitialState(); Rules.AddExperience(State,650);
    const auto Original=State;
    TestFalse(TEXT("Overspend rejected"),Rules.Invest(State,{{EPGStatType::Health,10}}));
    TestFalse(TEXT("Refund rejected"),Rules.Invest(State,{{EPGStatType::Attack,-1}}));
    TestFalse(TEXT("Unsupported stat rejected"),Rules.Invest(State,{{EPGStatType::CriticalRate,1}}));
    TestFalse(TEXT("Empty allocation rejected"),Rules.Invest(State,{}));
    TestEqual(TEXT("Rejected drafts preserve points"),State.UnspentPoints,Original.UnspentPoints);
    TestTrue(TEXT("Multiple investments commit together"),Rules.Invest(State,{{EPGStatType::Attack,2},{EPGStatType::Health,3}}));
    TestEqual(TEXT("Spend exact points"),State.UnspentPoints,4);
    TestEqual(TEXT("Authored attack gain"),Rules.InvestmentBonus(State,EPGStatType::Attack),4);
    TestEqual(TEXT("Authored health gain"),Rules.InvestmentBonus(State,EPGStatType::Health),60);
    TestTrue(TEXT("Conservation after investment"),Rules.Validate(State));
    Rules.InvestmentRules[EPGStatType::Attack].MaxPoints=2;
    TestFalse(TEXT("Per stat cap"),Rules.Invest(State,{{EPGStatType::Attack,1}}));
    Rules.InvestmentRules[EPGStatType::Defense].AmountPerPoint=7;
    TestTrue(TEXT("Tuned coefficient accepted"),Rules.Invest(State,{{EPGStatType::Defense,1}}));
    TestEqual(TEXT("Tuned coefficient applied"),Rules.InvestmentBonus(State,EPGStatType::Defense),7);
    const FString Slot=TEXT("PGInvestmentTest_")+FGuid::NewGuid().ToString();
    auto* Save=NewObject<UPGProfileSave>(); Save->Growth=State;
    TestTrue(TEXT("Allocation snapshot writes"),UGameplayStatics::SaveGameToSlot(Save,Slot,0));
    const auto* Reload=Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromSlot(Slot,0));
    TestTrue(TEXT("Allocation survives disk reload"),Reload && Reload->Growth.Investments.OrderIndependentCompareEqual(State.Investments) && Reload->Growth.UnspentPoints==State.UnspentPoints);
    UGameplayStatics::DeleteGameInSlot(Slot,0);
    Rules.InvestmentRules[EPGStatType::Health].MaxPoints=0;
    TestFalse(TEXT("Invalid investment data rejected"),Rules.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGLevelProgressionTest, "PG.Progression.LevelExperience", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGLevelProgressionTest::RunTest(const FString&)
{
    FPGLevelRules Rules;
    auto State = Rules.InitialState();
    TestTrue(TEXT("Multi level grant"), Rules.AddExperience(State, 650));
    TestEqual(TEXT("Three levels crossed"), State.Level, 4);
    TestEqual(TEXT("Remainder retained"), State.Experience, int64(50));
    TestEqual(TEXT("Every level grants points"), State.UnspentPoints, 9);
    TestFalse(TEXT("Negative XP rejected"), Rules.AddExperience(State, -1));
    TestTrue(TEXT("Huge award avoids integer overflow"), Rules.AddExperience(State, MAX_int64));
    TestEqual(TEXT("Maximum level"), State.Level, 20);
    TestEqual(TEXT("Maximum has no XP remainder"), State.Experience, int64(0));
    TestEqual(TEXT("Maximum grants exactly 19 levels"), State.EarnedPoints, 57);
    Rules.AddExperience(State, MAX_int64);
    TestEqual(TEXT("No points after maximum"), State.UnspentPoints, 57);
    --State.UnspentPoints;
    TestFalse(TEXT("Point conservation enforced"), Rules.Validate(State));
    State.Investments.Add(EPGStatType::Attack, 1);
    TestTrue(TEXT("Invested points counted"), Rules.Validate(State));
    Rules.Steps[0].RequiredExperience = 0;
    TestFalse(TEXT("Zero threshold rejected"), Rules.IsValid());
    Rules = FPGLevelRules();
    Rules.StartingLevel = 3; Rules.StartingPoints = 7;
    TestTrue(TEXT("Authored start valid"), Rules.Validate(Rules.InitialState()));

    auto* System = NewObject<UPGProfileSubsystem>(NewObject<UGameInstance>());
    System->TestSlotPrefix = TEXT("PGLevelTest_") + FGuid::NewGuid().ToString() + TEXT("_");
    System->Catalog = NewObject<UPGProgressionData>(System);
    System->Catalog->bRoguelikeRuns = true;
    FPGBuildDefinition Build; Build.Id = TEXT("Test"); System->Catalog->Builds.Add(Build);
    System->Profile = NewObject<UPGProfileSave>(System);
    System->Profile->Version = 4;
    System->Profile->RunId = FGuid::NewGuid(); System->Profile->RunSeed = 123;
    TestTrue(TEXT("Legacy fixture saves"), UGameplayStatics::SaveGameToSlot(System->Profile, System->SlotName(0), 0));
    System->bInjectSaveFailure = true;
    System->LoadProfile();
    TestTrue(TEXT("Migration failure blocks writes"), System->IsSaveBlocked());
    TestEqual(TEXT("Failed migration retains v4"), System->Profile->Version, 4);
    System->bInjectSaveFailure = false; System->LoadProfile();
    TestEqual(TEXT("Growth migration version"), System->Profile->Version, UPGProfileSave::CurrentVersion);
    TestEqual(TEXT("No inferred legacy experience"), System->Profile->Growth.Experience, int64(0));
    const FGuid Run = System->Profile->RunId;
    const FGuid Kill = FGuid::NewGuid();
    int32 Notifications = 0;
    System->OnProfileChanged.AddLambda([&Notifications](){ ++Notifications; });
    System->bInjectSaveFailure = true;
    TestFalse(TEXT("Failed kill write"), System->GrantExperience(Run, Kill, 650));
    TestEqual(TEXT("No state or notification before persistence"), Notifications, 0);
    TestEqual(TEXT("Failed write keeps level"), System->Profile->Growth.Level, 1);
    TestFalse(TEXT("Failed write has no level notice"),System->HasPendingLevelNotice());
    System->bInjectSaveFailure = false;
    const FGuid Reward = FGuid::NewGuid();
    TestTrue(TEXT("Reward atomically retries pending kill"), System->CommitReward(Reward, 2, EPGStatType::Attack, 1));
    TestEqual(TEXT("Pending XP and reward XP combined"), System->Profile->Growth.Experience, int64(100));
    TestEqual(TEXT("Only one committed notification"), Notifications, 1);
    TestTrue(TEXT("Multi level commit coalesces notice"),System->HasPendingLevelNotice());
    System->AcknowledgeLevelNotice();
    const int64 Revision = System->Profile->Revision;
    TestTrue(TEXT("Repeated kill is idempotent"), System->GrantExperience(Run, Kill, 650));
    TestEqual(TEXT("Duplicate makes no disk write"), System->Profile->Revision, Revision);
    TestFalse(TEXT("Duplicate does not reopen notice"),System->HasPendingLevelNotice());
    System->LoadProfile();
    TestEqual(TEXT("Disk reload retains points"), System->Profile->Growth.UnspentPoints, 9);
    TestEqual(TEXT("Reload does not grant levels again"), System->Profile->Revision, Revision);
    TestFalse(TEXT("Reload does not reopen notice"),System->HasPendingLevelNotice());
    TestTrue(TEXT("Resumed kill is idempotent"), System->GrantExperience(Run, Kill, 650));
    TestFalse(TEXT("Resumed reward rejected"), System->CommitReward(Reward, 2, EPGStatType::Attack, 1));
    TestFalse(TEXT("Previous run token rejected"), System->GrantExperience(FGuid::NewGuid(), Kill, 1));
    const FGuid Retry = FGuid::NewGuid();
    System->bInjectSaveFailure = true;
    TestFalse(TEXT("Failed grant queued"), System->GrantExperience(Run, Retry, 20));
    TestFalse(TEXT("Failed new run retains growth"), System->BeginNewRun(42));
    TestEqual(TEXT("Reset failure retains points"), System->Profile->Growth.UnspentPoints, 9);
    System->bInjectSaveFailure = false;
    TestTrue(TEXT("Same token retry succeeds"), System->GrantExperience(Run, Retry, 20));
    TestTrue(TEXT("New run commits"), System->BeginNewRun(42));
    TestEqual(TEXT("New run resets level"), System->Profile->Growth.Level, 1);
    TestEqual(TEXT("New run resets points"), System->Profile->Growth.UnspentPoints, 0);
    TestTrue(TEXT("New run clears ledger"), System->Profile->ExperienceClaims.IsEmpty());
    TestFalse(TEXT("Delayed old run grant rejected"), System->GrantExperience(Run, Retry, 20));
    TestTrue(TEXT("Maximum award persists"), System->GrantExperience(System->Profile->RunId, FGuid::NewGuid(), MAX_int64));
    System->LoadProfile();
    TestEqual(TEXT("Maximum survives reload"), System->Profile->Growth.Level, 20);
    TestTrue(TEXT("Run can end"), System->EndRun(false, 1));
    TestFalse(TEXT("Ended run cannot earn"), System->GrantExperience(System->Profile->RunId, FGuid::NewGuid(), 1));
    System->OnProfileChanged.Clear();
    for (int32 Index = 0; Index < 2; ++Index) UGameplayStatics::DeleteGameInSlot(System->SlotName(Index), 0);
    return true;
}
#endif
