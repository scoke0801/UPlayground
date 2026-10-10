#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "PGActor/Components/Rendering/PGToonPresentationComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGToonKeyLightTest, "PG.Rendering.Toon.KeyLightSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGToonKeyLightTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    TestNull(TEXT("Empty level uses analytic fallback"), UPGToonPresentationComponent::ResolveKeyLight(World));
    auto* Fill = World->SpawnActor<ADirectionalLight>();
    auto* Sun = World->SpawnActor<ADirectionalLight>();
    Fill->GetLightComponent()->SetIntensity(1.f);
    Sun->GetLightComponent()->SetIntensity(5.f);
    TestEqual(TEXT("Legacy level chooses strongest, not first spawned"), UPGToonPresentationComponent::ResolveKeyLight(World), Sun);
    Fill->Tags.Add(TEXT("PGToonKeyLight"));
    TestEqual(TEXT("Explicit level tag takes precedence"), UPGToonPresentationComponent::ResolveKeyLight(World), Fill);
    Fill->SetActorHiddenInGame(true);
    TestEqual(TEXT("Hidden tagged light does not illuminate characters"), UPGToonPresentationComponent::ResolveKeyLight(World), Sun);
    Fill->SetActorHiddenInGame(false);
    Fill->GetLightComponent()->SetVisibility(false);
    TestEqual(TEXT("Invisible tagged component is excluded"), UPGToonPresentationComponent::ResolveKeyLight(World), Sun);
    Fill->GetLightComponent()->SetVisibility(true);
    Fill->GetLightComponent()->SetIntensity(0.f);
    TestEqual(TEXT("Disabled light does not win selection"), UPGToonPresentationComponent::ResolveKeyLight(World), Sun);
    Sun->Destroy();
    TestNull(TEXT("Destroyed light is not retained by resolver"), UPGToonPresentationComponent::ResolveKeyLight(World));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGToonQualityTest, "PG.Rendering.Toon.DistanceQuality",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGToonQualityTest::RunTest(const FString&)
{
    TestEqual(TEXT("Portrait preserves all detail"), UPGToonPresentationComponent::CalculateDetailWeight(200, 600, 1600, .35f), 1.f);
    TestEqual(TEXT("Far view clamps to authored minimum"), UPGToonPresentationComponent::CalculateDetailWeight(3000, 600, 1600, .35f), .35f);
    float Previous = 1.f;
    for (int32 Distance = 600; Distance <= 1600; ++Distance)
    {
        const float Weight = UPGToonPresentationComponent::CalculateDetailWeight(Distance, 600, 1600, .35f);
        TestTrue(TEXT("Detail decreases continuously with distance"), Weight <= Previous + 1e-6f && Previous - Weight < .002f);
        Previous = Weight;
    }
    TestTrue(TEXT("Inverted ranges are finite and bounded"), FMath::IsFinite(UPGToonPresentationComponent::CalculateDetailWeight(900, 800, 400, .35f)));
    return true;
}
#endif
