#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "TimerManager.h"
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
    Stage->WaveStartTime = World->GetTimeSeconds();
    TestEqual(TEXT("Delayed first spawn waits"), Stage->SelectNextMonsterToSpawn(), INDEX_NONE);
    Stage->WaveStartTime -= 3.f;
    TestEqual(TEXT("Elapsed time unlocks first spawn even when none spawned"), Stage->SelectNextMonsterToSpawn(), 0);
    Stage->OnEnemyKilled(Enemy);
    Stage->OnEnemyKilled(Enemy);
    Stage->OnEnemyKilled(nullptr);
    TestEqual(TEXT("Duplicate and null deaths do not reduce count"), Stage->RemainingMonsters, 1);
    TestEqual(TEXT("Dead actor removed"), Stage->SpawnedEnemies.Num(), 0);
    FPGStageWave Wave;
    Wave.StartDelay = 2.f;
    Wave.MonsterSpawnInfos.Add(FPGMonsterSpawnInfo(1, 2));
    Stage->ActiveWaves = {Wave, Wave};
    Stage->CurrentWaveIndex = 0;
    Stage->RemainingMonsters = 0;
    Stage->CheckStageComplete();
    TestEqual(TEXT("Pending spawn prevents wave completion"), Stage->CurrentWaveIndex, 0);
    Stage->MonsterSpawnQueue.Reset();
    Stage->CheckStageComplete();
    TestEqual(TEXT("First clear advances wave, not stage"), Stage->GetCurrentWaveNumber(), 2);
    TestEqual(TEXT("Next wave waits for its start delay"), Stage->CurrentStageState, EPGStageState::WaveIntermission);
    TestEqual(TEXT("Next wave owns its own remaining count"), Stage->RemainingMonsters, 2);
    TestFalse(TEXT("No reward between waves"), Stage->RewardToken.IsValid());
    Stage->StartWave();
    TestEqual(TEXT("Wave clock resets at wave start"), Stage->WaveStartTime, static_cast<float>(World->GetTimeSeconds()));
    Stage->RemainingMonsters = 0;
    Stage->MonsterSpawnQueue.Reset();
    Stage->SpawnedEnemies.Add(Enemy);
    Stage->CheckStageComplete();
    TestEqual(TEXT("Tracked summons prevent early clear"), Stage->CurrentStageState, EPGStageState::InProgress);
    Stage->SpawnedEnemies.Reset();
    Stage->CheckStageComplete();
    TestEqual(TEXT("Final wave starts build phase"), Stage->CurrentStageState, EPGStageState::BuildPhase);
    TestTrue(TEXT("Build timer starts without waiting for reward selection"), Stage->GetBuildTimeRemaining() > 0.f);
    const float BuildRemaining = Stage->GetBuildTimeRemaining();
    Stage->RewardToken = FGuid::NewGuid();
    TestFalse(TEXT("Stale reward request rejected"), Stage->CommitReward(FGuid::NewGuid(), 0));
    Stage->OnRewardSelected();
    TestEqual(TEXT("Legacy completion cannot bypass grant"), Stage->CurrentStageState, EPGStageState::BuildPhase);
    Stage->bRewardCommitted = true;
    Stage->OnRewardSelected();
    TestEqual(TEXT("Reward selection keeps build time"), Stage->CurrentStageState, EPGStageState::BuildPhase);
    TestEqual(TEXT("Reward does not restart countdown"), Stage->GetBuildTimeRemaining(), BuildRemaining);
    Stage->OnRewardSelected();
    TestEqual(TEXT("Duplicate completion stays in build"), Stage->CurrentStageState, EPGStageState::BuildPhase);
    Stage->GoToNextStage();
    TestEqual(TEXT("Legacy advance cannot bypass build time"), Stage->CurrentStageState, EPGStageState::BuildPhase);
    AddExpectedError(TEXT("Player defeated."), EAutomationExpectedErrorFlags::Contains, 1);
    Stage->OnPlayerDied(nullptr);
    TestEqual(TEXT("Death during build fails stage"), Stage->CurrentStageState, EPGStageState::Failed);
    TestFalse(TEXT("Death clears build timer"), World->GetTimerManager().IsTimerActive(Stage->NextStageTimer));
    Stage->FinishBuildPhase();
    TestEqual(TEXT("Stale build callback cannot advance failed stage"), Stage->CurrentStageState, EPGStageState::Failed);
    Stage->CurrentStageDataCache.bManualReady = true;
    Stage->bRewardCommitted = false;
    Stage->BeginBuildPhase();
    TestFalse(TEXT("Manual preparation has no automatic countdown"),World->GetTimerManager().IsTimerActive(Stage->NextStageTimer));
    TestFalse(TEXT("Cannot ready before choosing reward"),Stage->CanReady());
    Stage->ReadyForNextStage();
    TestEqual(TEXT("Premature ready does not advance"),Stage->CurrentStageState,EPGStageState::BuildPhase);
    Stage->bRewardCommitted = true;
    TestTrue(TEXT("Committed reward unlocks ready"),Stage->CanReady());
    Stage->PrepareRun(1);
    TestTrue(TEXT("Starting loadout preparation can launch"),Stage->CanReady());
    TestEqual(TEXT("Preparation does not spawn combat"),Stage->CurrentStageState,EPGStageState::RunPreparation);
    World->DestroyWorld(false);
    return true;
}
#endif
