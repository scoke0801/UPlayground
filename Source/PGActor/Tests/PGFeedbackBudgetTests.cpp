#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "NiagaraSystem.h"
#include "PGData/DataAsset/Combat/PGCombatFeedbackData.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGFeedbackBudgetTest, "PG.CombatCycle.HitStopBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGFeedbackBudgetTest::RunTest(const FString& Parameters)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* Actor=World->SpawnActor<APGCharacterBase>();
    Actor->DispatchBeginPlay();
    Actor->GetMesh()->GlobalAnimRateScale=.8f;
    Actor->ApplyHitStop(.03f);
    for(int32 I=0; I<100; ++I) Actor->ApplyHitStop(.15f);
    TestEqual(TEXT("Burst freezes animation once"),Actor->GetMesh()->GlobalAnimRateScale,0.f);
    TestEqual(TEXT("Burst cannot extend hit stop"),Actor->GetTotalHitStopSeconds(),.03f);
    TestEqual(TEXT("Suppressed requests are inspectable"),Actor->GetSuppressedFeedbackRequests(),100);
    Actor->EndHitStop();
    TestEqual(TEXT("Original animation speed restored"),Actor->GetMesh()->GlobalAnimRateScale,.8f);
    auto* Attacker=World->SpawnActor<APGCharacterBase>();
    Attacker->NextImpactVFXAt=10.;
    Actor->FeedbackData=NewObject<UPGCombatFeedbackData>();
    Actor->FeedbackData->Normal.VFX=NewObject<UNiagaraSystem>();
    Actor->FeedbackData->Normal.HitStop=.03f;
    Actor->FeedbackData->MinimumSourceVFXInterval=.08f;
    Actor->PlayCombatFeedback(Attacker,EPGDamageType::Normal);
    TestEqual(TEXT("Suppressed crowd VFX preserves victim hit stop"),Actor->GetMesh()->GlobalAnimRateScale,0.f);
    TestEqual(TEXT("Suppressed crowd VFX preserves attacker hit stop"),Attacker->GetMesh()->GlobalAnimRateScale,0.f);
    TestEqual(TEXT("Visual suppression does not extend its budget"),Attacker->NextImpactVFXAt,10.);
    Actor->EndHitStop();
    Actor->ApplyHitStop(.02f); Actor->Destroy();
    TestFalse(TEXT("Destroyed actor leaves no hit stop timer"),World->GetTimerManager().TimerExists(Actor->HitStopTimer));
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return true;
}
#endif


