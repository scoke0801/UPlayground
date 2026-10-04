#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGAI/Task/PGBTTask_FindSkillUseLocation.h"
#include "PGAI/Task/PGBTTask_ExecuteSkill.h"
#include "PGAI/PGRoleAIController.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/Abilities/PGGameplayAbility.h"
#include "PGAbilitySystem/Abilities/PGEnemyGameplayAbility.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Engine/World.h"

namespace
{
UWorld* CreateBTTestWorld()
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGBTAsyncStateTest, "PG.AI.BT.AsyncQueryIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGBTAsyncStateTest::RunTest(const FString&)
{
    auto* World = CreateBTTestWorld();
    auto* A = World->SpawnActor<AAIController>();
    auto* B = World->SpawnActor<AAIController>();
    auto* ComponentA = NewObject<UBehaviorTreeComponent>(A);
    auto* ComponentB = NewObject<UBehaviorTreeComponent>(B);
    auto* Template = NewObject<UPGBTTask_FindSkillUseLocation>();
    TestTrue(TEXT("Shared EQS template requires per-agent instances"), Template->HasInstance());
    auto* First = DuplicateObject(Template, ComponentA);
    auto* Second = DuplicateObject(Template, ComponentB);
    First->CachedOwnerComp = ComponentA; First->QueryID = 11; First->QuerySkillID = 15102;
    Second->CachedOwnerComp = ComponentB; Second->QueryID = 12; Second->QuerySkillID = 15112;
    auto LateResult = MakeShared<FEnvQueryResult>(); LateResult->QueryID = 10;
    First->OnEQSQueryFinished(LateResult);
    TestEqual(TEXT("Late completion cannot replace the new request"), First->QueryID, 11);
    TestEqual(TEXT("Abort completes synchronously"), First->AbortTask(*ComponentA, nullptr), EBTNodeResult::Aborted);
    TestEqual(TEXT("Abort invalidates request identity"), First->QueryID, INDEX_NONE);
    LateResult->QueryID = 11;
    First->OnEQSQueryFinished(LateResult);
    TestFalse(TEXT("Post-abort callback cannot regain ownership"), First->CachedOwnerComp.IsValid());
    TestEqual(TEXT("Aborting one agent preserves the other query"), Second->QueryID, 12);
    TestTrue(TEXT("Other agent retains its own BT component"), Second->CachedOwnerComp == ComponentB);
    Second->OnInstanceDestroyed(*ComponentB);
    TestEqual(TEXT("Tree destruction invalidates outstanding request"), Second->QueryID, INDEX_NONE);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGBTAbilityLifecycleTest, "PG.AI.BT.AbilityOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGBTAbilityLifecycleTest::RunTest(const FString&)
{
    auto* World = CreateBTTestWorld();
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>();
    auto* ASC = Enemy->GetPGAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Enemy, Enemy);
    ASC->InitializeCombatStats({{EPGStatType::Health, 100}});
    const auto Attack = ASC->GiveAbility(FGameplayAbilitySpec(UPGGameplayAbility::StaticClass(), 1));
    const auto Other = ASC->GiveAbility(FGameplayAbilitySpec(UPGEnemyGameplayAbility::StaticClass(), 1));
    TestTrue(TEXT("Owned native ability activates"), ASC->TryActivateAbility(Attack));
    TestTrue(TEXT("Independent native ability activates"), ASC->TryActivateAbility(Other));
    auto* Task = NewObject<UPGBTTask_ExecuteSkill>();
    Task->CachedASC = ASC; Task->ActiveAbilityHandle = Attack;
    Task->AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(Task, &UPGBTTask_ExecuteSkill::OnAbilityEnded);
    ASC->CancelAbilityHandle(Other);
    TestTrue(TEXT("Other ability ending does not complete the attack task"), Task->ActiveAbilityHandle == Attack);
    Task->bActivating = true;
    ASC->CancelAbilityHandle(Attack);
    TestTrue(TEXT("Synchronous cancellation is deferred until ExecuteTask returns"), Task->bEndedDuringActivation);
    TestTrue(TEXT("Cancellation is retained as failure"), Task->bEndedCancelled);
    Task->ResetExecution(false);
    TestTrue(TEXT("Attack can start again after cleanup"), ASC->TryActivateAbility(Attack));
    TestTrue(TEXT("Independent ability can start again"), ASC->TryActivateAbility(Other));
    Task->CachedASC = ASC; Task->ActiveAbilityHandle = Attack;
    Task->AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(Task, &UPGBTTask_ExecuteSkill::OnAbilityEnded);
    Task->ResetExecution(true);
    TestFalse(TEXT("BT abort cancels the owned ability"), ASC->FindAbilitySpecFromHandle(Attack)->IsActive());
    TestTrue(TEXT("BT abort preserves unrelated ability"), ASC->FindAbilitySpecFromHandle(Other)->IsActive());
    TestFalse(TEXT("Abort removes the end delegate before cancellation"), Task->AbilityEndedHandle.IsValid());
    // Repeated cleanup must never cancel a different activation.
    Task->ResetExecution(true);
    TestTrue(TEXT("Repeated cleanup is harmless"), ASC->FindAbilitySpecFromHandle(Other)->IsActive());
    ASC->CancelAbilityHandle(Other);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCombatBehaviorTreeTest, "PG.AI.BT.RoleTreeLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCombatBehaviorTreeTest::RunTest(const FString&)
{
    auto* World = CreateBTTestWorld();
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>();
    auto* AI = World->SpawnActor<APGRoleAIController>();
    AI->Possess(Enemy);
    TestTrue(TEXT("Runtime role tree loads through the engine BT manager"), AI->StartCombatBehaviorTree());
    TestTrue(TEXT("Pilot uses BT driver"), AI->IsUsingCombatBehaviorTree());
    auto* OtherEnemy = World->SpawnActor<APGCharacterEnemy>();
    auto* OtherAI = World->SpawnActor<APGRoleAIController>();
    OtherAI->Possess(OtherEnemy);
    TestTrue(TEXT("Second agent starts the common tree"), OtherAI->StartCombatBehaviorTree());
    TestTrue(TEXT("Agents share a world-owned template"), AI->CombatBehaviorTree == OtherAI->CombatBehaviorTree);
    TestTrue(TEXT("Agents have independent blackboards"), AI->GetBlackboardComponent() != OtherAI->GetBlackboardComponent());
    auto* BT = Cast<UBehaviorTreeComponent>(AI->GetBrainComponent());
    TestNotNull(TEXT("Real BT component exists"), BT);
    TestNotNull(TEXT("Blackboard exists"), AI->GetBlackboardComponent());
    if (BT)
    {
        TestTrue(TEXT("Tree starts"), BT->IsRunning());
        AI->SetCombatThinkingEnabled(false);
        TestFalse(TEXT("Presentation/QA pause stops tree"), BT->IsRunning());
        AI->SetCombatThinkingEnabled(true);
        TestTrue(TEXT("Resuming restarts tree"), BT->IsRunning());
        AI->UnPossess();
        TestFalse(TEXT("Unpossess stops the tree"), BT->IsRunning());
    }
    TestFalse(TEXT("Unpossess clears pilot driver state"), AI->IsUsingCombatBehaviorTree());
    OtherAI->UnPossess();
    World->DestroyWorld(false);
    return true;
}
#include "PGAI/PGCombatSpatial.h"
#include "PGAI/PGCombatTreeLibrary.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCombatPositionTest, "PG.AI.BT.PositionStability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCombatPositionTest::RunTest(const FString& Parameters)
{
    const FVector Center(0,0,0), Origin(200,0,0), Side(0,200,0);
    const TArray<FVector> Empty;
    const float Stay = PGCombatSpatial::PositionCost(Origin, Origin, Center, 200, 150, Empty);
    TestTrue(TEXT("An uncontested valid position beats unnecessary orbiting"), Stay < PGCombatSpatial::PositionCost(Side, Origin, Center, 200, 150, Empty));
    const TArray<FVector> Crowded = {Origin + FVector(20,0,0)};
    TestTrue(TEXT("An open flank beats overlapping another enemy"), PGCombatSpatial::PositionCost(Side, Origin, Center, 200, 150, Crowded) + 45.f < PGCombatSpatial::PositionCost(Origin, Origin, Center, 200, 150, Crowded));
    TestTrue(TEXT("Correct firing range beats an unnecessarily distant slot"), Stay < PGCombatSpatial::PositionCost(FVector(600,0,0), Origin, Center, 200, 150, Empty));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCombatGraphTest, "PG.AI.BT.EditableGraph", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCombatGraphTest::RunTest(const FString& Parameters)
{
    auto* Tree = NewObject<UBehaviorTree>();
    auto* Board = NewObject<UBlackboardData>();
    TestTrue(TEXT("Build authored graph from runtime policy"), UPGCombatTreeLibrary::ConfigureEditorTree(Tree, Board, .2f));
    TestTrue(TEXT("Graph reconstructs the expected executable branches"), UPGCombatTreeLibrary::ValidateCombatTree(Tree));
    TestTrue(TEXT("Rebuilding remains valid"), UPGCombatTreeLibrary::ConfigureEditorTree(Tree, Board, .3f));
    TestFalse(TEXT("Invalid interval cannot overwrite the graph"), UPGCombatTreeLibrary::ConfigureEditorTree(Tree, Board, 0.f));
    return true;
}
#endif
