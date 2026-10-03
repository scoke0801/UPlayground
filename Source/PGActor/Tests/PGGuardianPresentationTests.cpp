#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGEnemyPresentationComponent.h"
#include "PGData/DataAsset/Combat/PGEnemyPresentationData.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

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
#endif
