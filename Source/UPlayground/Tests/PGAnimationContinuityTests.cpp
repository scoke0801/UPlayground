#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AnimInstances/PGCharacterAnimInstance.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGAnimationHitStopDeltaTest, "PG.Animation.HitStopDelta",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGAnimationHitStopDeltaTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* Player = World->SpawnActor<APGCharacterPlayer>();
    auto* Anim = NewObject<UPGCharacterAnimInstance>(Player->GetMesh());
    Anim->OwningCharacter = Player;
    Anim->CurrentWorldLocation = Player->GetActorLocation();
    Anim->UpdateDisplacementSpeed(0.f);
    TestEqual(TEXT("Stationary hit-stop produces finite zero, not 0/0"), Anim->DisplacementSpeed, 0.f);
    Player->SetActorLocation(FVector(30,0,0));
    Anim->UpdateDisplacementSpeed(0.f);
    TestEqual(TEXT("Moved frozen pose never produces infinity"), Anim->DisplacementSpeed, 0.f);
    Player->SetActorLocation(FVector(40,0,0));
    Anim->UpdateDisplacementSpeed(.1f);
    TestEqual(TEXT("Resume consumes only movement since last pose update"), Anim->DisplacementSpeed, 100.f, .001f);
    World->DestroyWorld(false);
    return true;
}
#endif
