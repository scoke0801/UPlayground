#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGActor/Progression/PGRunTelemetrySubsystem.h"
#include "PGShared/Shared/Structure/PGRunRandom.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRunTelemetryTest, "PG.Run.TelemetryAndSeed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGRunTelemetryTest::RunTest(const FString& Parameters)
{
    FRandomStream Spawn(PGRunRandom::Seed(1234, 4, 2, 1));
    FRandomStream Rewards(PGRunRandom::Seed(1234, 4, 0, 2, 1));
    for (int32 I = 0; I < 100; ++I) Spawn.FRand();
    FRandomStream RestoredRewards(PGRunRandom::Seed(1234, 4, 0, 2, 1));
    for (int32 I = 0; I < 10; ++I)
        TestEqual(TEXT("Spawn retries do not perturb restored reward draws"), Rewards.FRand(), RestoredRewards.FRand());
    TestNotEqual(TEXT("Second card selection uses a distinct stream"), PGRunRandom::Seed(1234,4,0,2,0), PGRunRandom::Seed(1234,4,0,2,1));

    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* Telemetry = World->GetSubsystem<UPGRunTelemetrySubsystem>();
    if (!TestNotNull(TEXT("World owns telemetry"), Telemetry)) { World->DestroyWorld(false); return false; }
    Telemetry->ActiveSample = Telemetry->Samples.AddDefaulted();
    auto* Player = World->SpawnActor<APGCharacterPlayer>();
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>();
    auto* Source = Player->GetPGAbilitySystemComponent();
    auto* Target = Enemy->GetPGAbilitySystemComponent();
    Source->InitAbilityActorInfo(Player, Player); Target->InitAbilityActorInfo(Enemy, Enemy);
    Source->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Attack,100}});
    Target->InitializeCombatStats({{EPGStatType::Health,200},{EPGStatType::Defense,0},{EPGStatType::Attack,10}});
    EPGDamageType Type;
    Target->ReceiveCombatHit(Source, Type);
    Source->ReceiveCombatHit(Target, Type);
    Target->ReceiveProcDamage(Source, 1000.f);
    TestEqual(TEXT("Direct damage counted once"), Telemetry->Samples[0].DirectDamage, 100.);
    TestEqual(TEXT("Proc excludes overkill"), Telemetry->Samples[0].SecondaryDamage, 100.);
    TestEqual(TEXT("Incoming damage kept separate"), Telemetry->Samples[0].DamageTaken, 10.);
    Target->ReceiveProcDamage(Source, 100.f);
    TestEqual(TEXT("Already dead target adds no damage"), Telemetry->Samples[0].SecondaryDamage, 100.);
    const int32 BeforeTransition = Telemetry->GetSampleIndex();
    Telemetry->ActiveSample = Telemetry->Samples.AddDefaulted();
    Telemetry->RecordDamage(BeforeTransition, 7.f, true, false, true);
    TestEqual(TEXT("Lethal callback settlement belongs to original stage"), Telemetry->Samples[0].SecondaryDamage, 107.);
    TestEqual(TEXT("Next stage is not contaminated"), Telemetry->Samples[1].SecondaryDamage, 0.);
    Telemetry->MarkAssisted();
    TestTrue(TEXT("Assisted covers earlier samples in this run"), Telemetry->Samples[0].bAssisted && Telemetry->Samples[1].bAssisted);
    World->DestroyWorld(false);
    return true;
}
#endif
