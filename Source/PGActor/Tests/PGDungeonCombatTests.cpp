#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGDungeonCombatTest, "PG.Dungeon.CombatGuards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGDungeonCombatTest::RunTest(const FString& Parameters)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* Stage = World->SpawnActor<APGStageManager>();
    TestFalse(TEXT("Incomplete mapping is rejected before save access"), Stage->ConfigureDungeonCombat({},1400,350,500,15,1));
    Stage->bDungeonCombat = true;
    Stage->DungeonHalfSize = 1400;
    Stage->DungeonEntryInset = 350;
    Stage->DungeonRooms = {FVector(3800,0,0), FVector(7600,0,0), FVector(11400,0,0),
        FVector(15200,0,0), FVector(19000,0,0), FVector(22800,0,0)};
    Stage->AwaitDungeonObjective(1);
    TestTrue(TEXT("Room interior starts objective"), Stage->IsInsideDungeonObjective(FVector(3800,0,100),350));
    TestFalse(TEXT("Doorway does not close on player"), Stage->IsInsideDungeonObjective(FVector(2450,0,100),350));
    TestFalse(TEXT("Entrance cannot spawn encounter"), Stage->IsInsideDungeonObjective(FVector(0,0,100),350));
    TestFalse(TEXT("Fallen player cannot trigger encounter"), Stage->IsInsideDungeonObjective(FVector(3800,0,-500),350));
    Stage->StartStage(6);
    TestEqual(TEXT("Direct boss request cannot skip prerequisites"), Stage->CurrentStageId,1);
    TestEqual(TEXT("Rejected request keeps traversal"), Stage->CurrentStageState, EPGStageState::DungeonTraversal);
    Stage->ReadyForNextStage();
    TestEqual(TEXT("Ready button cannot start encounter outside room"), Stage->CurrentStageState, EPGStageState::DungeonTraversal);
    Stage->CurrentStageState=EPGStageState::Completed;
    Stage->GoToNextStage();
    TestEqual(TEXT("Completion awaits next room without starting waves"), Stage->CurrentStageState, EPGStageState::DungeonTraversal);
    TestEqual(TEXT("Objective advances once"), Stage->CurrentStageId,2);
    Stage->GoToNextStage();
    TestEqual(TEXT("Duplicate completion cannot advance"), Stage->CurrentStageId,2);
    auto* Enemy=World->SpawnActor<APGCharacterEnemy>();
    Stage->CurrentStageState=EPGStageState::InProgress;
    Stage->SpawnedEnemies.Add(Enemy);
    Stage->RemainingMonsters=1;
    Stage->RewardToken=FGuid::NewGuid();
    AddExpectedError(TEXT("Dungeon enemy disappeared"),EAutomationExpectedErrorFlags::Contains,1);
    Stage->OnTrackedEnemyDestroyed(Enemy);
    TestEqual(TEXT("Unexpected removal cancels without awarding clear"), Stage->CurrentStageState,EPGStageState::Failed);
    TestFalse(TEXT("Failure invalidates reward capability"),Stage->RewardToken.IsValid());
    TestTrue(TEXT("Failure removes tracked enemies"),Stage->SpawnedEnemies.IsEmpty());
    Stage->StopDungeonCombat();
    Stage->StartStage(1);
    TestEqual(TEXT("Cancelled host cannot be restarted by stale calls"),Stage->CurrentStageState,EPGStageState::None);
    World->DestroyWorld(false);
    return true;
}
#endif
