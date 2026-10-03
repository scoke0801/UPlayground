#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGAI/PGCombatDirectorSubsystem.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCombatDirectorTest, "PG.AI.CombatPressure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCombatDirectorTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* Director = NewObject<UPGCombatDirectorSubsystem>(World);
    auto* Target = World->SpawnActor<AActor>();
    auto* OtherTarget = World->SpawnActor<AActor>();
    TArray<APGCharacterEnemy*> Enemies;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        auto* Enemy = World->SpawnActor<APGCharacterEnemy>();
        auto* ASC = Enemy->GetPGAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Enemy, Enemy); ASC->InitializeCombatStats({{EPGStatType::Health,100}});
        Enemy->bPatternActive = true; Enemies.Add(Enemy);
    }
    auto* A = Enemies[0]; auto* B = Enemies[1]; auto* C = Enemies[2];
    TestTrue(TEXT("First attack obtains capacity"), Director->TryReserveAt(A, Target, 2, 1., 3, .28f));
    TestFalse(TEXT("Warnings cannot start on the same beat"), Director->TryReserveAt(B, Target, 2, 1.1, 3, .28f));
    TestFalse(TEXT("Heavy attack waits for enough capacity"), Director->TryReserveAt(B, Target, 2, 1.3, 3, .28f));
    TestFalse(TEXT("New light attack cannot starve a queued heavy attack"), Director->TryReserveAt(C, Target, 1, 1.4, 3, .28f));
    A->bPatternActive = false;
    TestTrue(TEXT("Finished attack releases capacity without a tick or callback"), Director->TryReserveAt(B, Target, 2, 1.5, 3, .28f));
    TestTrue(TEXT("Independent targets have independent budgets"), Director->TryReserveAt(A, OtherTarget, 3, 1.6, 3, .28f));
    A->bPatternActive = true;
    TestTrue(TEXT("Remaining light slot opens after spacing"), Director->TryReserveAt(C, Target, 1, 1.9, 3, .28f));
    Director->Release(B); Director->Release(C); Director->Release(A);
    TestTrue(TEXT("Cancel/unpossess explicitly frees a reservation"), Director->TryReserveAt(A, Target, 3, 2.5, 3, .28f));
    A->GetPGAbilitySystemComponent()->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 0.f);
    TestTrue(TEXT("Dead attackers cannot retain pressure"), Director->TryReserveAt(B, Target, 3, 3., 3, .28f));
    Director->Release(B);
    TestTrue(TEXT("Reduced debug budget does not make bosses permanently ineligible"), Director->TryReserveAt(C, Target, 3, 4., 1, .28f));
    World->DestroyWorld(false);
    return true;
}
#endif
