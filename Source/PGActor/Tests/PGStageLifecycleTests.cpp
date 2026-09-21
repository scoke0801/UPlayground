#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGStageLifecycleTest, "PG.Stage.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGStageLifecycleTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    APGStageManager* Stage = World->SpawnActor<APGStageManager>();
    APGCharacterEnemy* Enemy = World->SpawnActor<APGCharacterEnemy>();
    Stage->CurrentStageState = EPGStageState::InProgress;
    Stage->RemainingMonsters = 2;
    Stage->SpawnedEnemies.Add(Enemy);
    Stage->MonsterSpawnQueue.Add(FPGMonsterSpawnQueueItem(1, 1, 2.f, 0));
    Stage->StageStartTime = World->GetTimeSeconds();
    TestEqual(TEXT("Delayed first spawn waits"), Stage->SelectNextMonsterToSpawn(), INDEX_NONE);
    Stage->StageStartTime -= 3.f;
    TestEqual(TEXT("Elapsed time unlocks first spawn even when none spawned"), Stage->SelectNextMonsterToSpawn(), 0);
    Stage->OnEnemyKilled(Enemy);
    Stage->OnEnemyKilled(Enemy);
    Stage->OnEnemyKilled(nullptr);
    TestEqual(TEXT("Duplicate and null deaths do not reduce count"), Stage->RemainingMonsters, 1);
    TestEqual(TEXT("Dead actor removed"), Stage->SpawnedEnemies.Num(), 0);
    Stage->CurrentStageState = EPGStageState::RewardPhase;
    Stage->RewardToken = FGuid::NewGuid();
    TestFalse(TEXT("Stale reward request rejected"), Stage->CommitReward(FGuid::NewGuid(), 0));
    Stage->OnRewardSelected();
    TestEqual(TEXT("Legacy completion cannot bypass grant"), Stage->CurrentStageState, EPGStageState::RewardPhase);
    Stage->bRewardCommitted = true;
    Stage->OnRewardSelected();
    TestEqual(TEXT("Committed reward completes"), Stage->CurrentStageState, EPGStageState::Completed);
    Stage->OnRewardSelected();
    TestEqual(TEXT("Duplicate completion stays complete"), Stage->CurrentStageState, EPGStageState::Completed);
    World->DestroyWorld(false);
    return true;
}
#endif