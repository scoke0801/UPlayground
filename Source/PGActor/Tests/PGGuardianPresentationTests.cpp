#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGEnemyPresentationComponent.h"
#include "PGData/DataAsset/Combat/PGEnemyPresentationData.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGGuardianPresentationLifecycleTest, "PG.Content.GuardianPresentationLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGGuardianPresentationLifecycleTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
    const auto Advance = [&](float Seconds)
    {
        for (int32 Index = 0; Index < FMath::CeilToInt(Seconds / .05f); ++Index)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, .05f);
        }
    };
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>();
    auto* Presentation = Enemy->GetEnemyPresentation();
    Presentation->Initialize(NewObject<UPGEnemyPresentationData>());
    Enemy->SetGuarding(true);
    Presentation->BeginWindup(.8f, .2f);
    Advance(.35f);
    TestTrue(TEXT("Aim lock is signalled during the authored windup"), Presentation->IsAimLocked());
    Enemy->SetGuarding(false);
    Presentation->BeginRecovery();
    Advance(.3f);
    TestEqual(TEXT("Recovery lowers the cosmetic shield"), Presentation->GetExposureAlpha(), 1.f);
    TestFalse(TEXT("Recovery clears the lock cue"), Presentation->IsAimLocked());
    Presentation->ResetPresentation();
    Enemy->SetGuarding(true);
    Advance(.3f);
    TestEqual(TEXT("A new guarding decision raises the shield again"), Presentation->GetExposureAlpha(), 0.f);
    Presentation->BeginWindup(.8f, .2f);
    Presentation->ResetPresentation(true);
    Enemy->SetGuarding(true); // A late AI event after death must not revive armor/cues.
    Presentation->BeginWindup(.8f, .2f);
    Advance(1.f);
    TestFalse(TEXT("Death cancels delayed lock cues"), Presentation->IsAimLocked());
    TestEqual(TEXT("Late events cannot raise a dead enemy's shield"), Presentation->GetExposureAlpha(), 1.f);
    Enemy->Destroy();
    Advance(.2f); // EndPlay clears timers before the owner becomes invalid.
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);

    FPGSkillDataRow Pattern;
    Pattern.TelegraphDuration = 1.f;
    Pattern.bSyncMontageToPattern = true;
    TestTrue(TEXT("Valid presentation sync retains the gameplay pattern"), Pattern.IsPatternValid());
    Pattern.ImpactMontageFraction = Pattern.WindupMontageFraction;
    TestFalse(TEXT("Zero anticipation segment is rejected before activation"), Pattern.IsPatternValid());
    Pattern.ImpactMontageFraction = 1.f;
    TestFalse(TEXT("Missing recovery animation segment is rejected"), Pattern.IsPatternValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGEnemyHitRecoilTest, "PG.CombatCycle.EnemyHitRecoil",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGEnemyHitRecoilTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
    const auto Advance = [&]() { for (int32 I = 0; I < 10; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .025f); } };
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>();
    auto* Presentation = Enemy->GetEnemyPresentation();
    auto* Mesh = Enemy->GetMesh();
    const FVector Before = Mesh->GetRelativeLocation();
    const FVector CapsuleBefore = Enemy->GetCapsuleComponent()->GetComponentLocation();
    Enemy->bPatternActive = true;
    Mesh->GlobalAnimRateScale = 0.f;
    Presentation->PlayHitRecoil(FVector::ForwardVector, 6.f, .14f);
    TestTrue(TEXT("Confirmed contact moves the mesh even during a committed attack and hit-stop"),
        Mesh->GetRelativeLocation().Equals(Before + FVector(6,0,0)));
    for (int32 I = 0; I < 100; ++I) Presentation->PlayHitRecoil(FVector::ForwardVector, 6.f, .14f);
    TestTrue(TEXT("Repeated contact never accumulates mesh displacement"), Mesh->GetRelativeLocation().Equals(Before + FVector(6,0,0)));
    TestTrue(TEXT("Capsule and gameplay position are unchanged"), Enemy->GetCapsuleComponent()->GetComponentLocation().Equals(CapsuleBefore));
    TestTrue(TEXT("Recoil does not cancel a committed attack"), Enemy->bPatternActive);
    Advance();
    TestTrue(TEXT("The original mesh offset returns after the pulse"), Mesh->GetRelativeLocation().Equals(Before));
    TestEqual(TEXT("Recoil never restores an animation rate owned by hit-stop"), Mesh->GlobalAnimRateScale, 0.f);
    Presentation->PlayHitRecoil(FVector::ForwardVector, 0.f, .14f);
    TestTrue(TEXT("Zero tuning disables recoil"), Mesh->GetRelativeLocation().Equals(Before));
    // Retargeted meshes may have a scaled parent: the authored distance is in world units.
    Enemy->SetActorScale3D(FVector(2));
    Presentation->PlayHitRecoil(FVector::ForwardVector, 6.f, .14f);
    TestTrue(TEXT("Parent scale cannot amplify the authored recoil distance"), Mesh->GetRelativeLocation().Equals(Before + FVector(3,0,0)));
    Presentation->ResetPresentation(true); // No armor data required for death cleanup.
    Presentation->PlayHitRecoil(FVector::ForwardVector, 6.f, .14f);
    Advance();
    TestTrue(TEXT("Death restores the mesh and rejects late impacts without armor data"), Mesh->GetRelativeLocation().Equals(Before));
    Enemy->Destroy();
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
#endif
