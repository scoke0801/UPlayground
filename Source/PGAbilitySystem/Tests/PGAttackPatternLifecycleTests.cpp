#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGAbilitySystem/Abilities/Combat/PGEnemyAbilityAttack.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGActor/Components/Combat/PGEnemyCombatComponent.h"
#include "PGActor/Projectile/PGPatternProjectile.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGAttackPatternLifecycleTest, "PG.Content.PatternLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGAttackPatternLifecycleTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>();
    auto* Player = World->SpawnActor<APGCharacterPlayer>();
    auto* Controller = World->SpawnActor<APGPlayerController>();
    Controller->Possess(Player);
    auto* AI = World->SpawnActor<AAIController>(); AI->SetGenericTeamId(FGenericTeamId(1)); AI->Possess(Enemy);
    auto* Source = Enemy->GetPGAbilitySystemComponent();
    auto* Target = Player->GetPGAbilitySystemComponent();
    Source->InitAbilityActorInfo(Enemy, Enemy); Target->InitAbilityActorInfo(Player, Player);
    Source->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Attack,100}});
    Target->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Defense,0}});
    Enemy->SetActorLocation(FVector(0,0,90));
    Player->SetActorLocation(FVector(150,0,90));
    Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Player->GetCharacterMovement()->DisableMovement();

    const auto Handle = Source->GiveAbility(FGameplayAbilitySpec(UPGEnemyAbilityAttack::StaticClass(), 1));
    auto* Ability = Cast<UPGEnemyAbilityAttack>(Source->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!TestNotNull(TEXT("Instanced pattern ability"), Ability)) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
    FPGSkillDataRow Row; Row.TelegraphDuration = 1.f; Row.TelegraphRadius = 200; Row.Pattern = EPGAttackPattern::Sweep;
    Row.RecoveryDuration = .5f; Row.AimTrackingSeconds = .2f;
    Row.bHeavyImpactFeedback = false; // Guardian-sized feedback must retain gameplay suppression.
    // Isolate the timed pattern from content lookup; GAS owns activation, actor info and cancellation.
    const auto Start = [&]()
    {
        Ability->PreActivate(Handle, Source->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), nullptr);
        Ability->UPGGameplayAbility::ActivateAbility(Handle, Source->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), nullptr);
        Ability->BeginElitePattern(Row);
        // This isolated world has no LocalPlayer/GameInstance; provide the possessed target explicitly.
        Ability->PatternTarget = Player;
        Ability->UpdateAim();
    };
    Start();
    TestTrue(TEXT("Windup blocks movement"), Enemy->bPatternActive && Enemy->GetCharacterMovement()->MovementMode == MOVE_None);
    TestTrue(TEXT("Small impact feedback preserves the active attack guard"), Enemy->bPerformingHeavyAttack && !Enemy->bUseHeavyImpactFeedback);
    TestEqual(TEXT("Windup does not damage"), Target->GetHealth(), 1000.f);
    // An animation notify must not add a second hit outside the timed shape.
    auto* Combat = Cast<UPGEnemyCombatComponent>(Enemy->GetCombatComponent());
    Combat->ToggleWeaponCollision(true, EPGToggleDamageType::LeftHand);
    TestEqual(TEXT("Pattern suppresses body notify collision"), Enemy->GetLeftHandCollisionBox()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    Combat->OnHitTargetActor(Player);
    TestEqual(TEXT("Pattern suppresses notify damage"), Target->GetHealth(), 1000.f);
    Ability->StrikeElitePattern();
    TestEqual(TEXT("One strike applies one GAS hit despite multiple components"), Target->GetHealth(), 900.f);
    TestTrue(TEXT("Recovery exposes the attacker"), Enemy->bPatternRecovering && Source->IsRecoveryExposed());
    Source->CancelAbilityHandle(Handle);
    TestFalse(TEXT("Cancel closes the recovery window"), Source->IsRecoveryExposed());
    TestFalse(TEXT("Cancel clears update timer"), World->GetTimerManager().TimerExists(Ability->UpdateTimer));
    TestFalse(TEXT("Cancel clears delayed damage timer"), World->GetTimerManager().TimerExists(Ability->PatternTimer));
    TestEqual(TEXT("Cancel restores movement"), Enemy->GetCharacterMovement()->MovementMode, TEnumAsByte<EMovementMode>(MOVE_Walking));
    TestFalse(TEXT("Cancel clears active pattern"), Enemy->bPatternActive);
    TestTrue(TEXT("Cancel restores default feedback selection"), Enemy->bUseHeavyImpactFeedback);
    TestNull(TEXT("Cancel clears decal"), Ability->Telegraph.Get());

    Row.Pattern = EPGAttackPattern::HazardSequence;
    Start(); Ability->StrikeElitePattern();
    const float AfterFirstHazard = Target->GetHealth();
    TestTrue(TEXT("Hazard schedules the next pulse"), World->GetTimerManager().TimerExists(Ability->PatternTimer));
    Source->CancelAbilityHandle(Handle);
    World->GetTimerManager().Tick(2.f);
    TestEqual(TEXT("Canceled hazards cannot hit later"), Target->GetHealth(), AfterFirstHazard);

    Start(); Target->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 0.f);
    Ability->StrikeElitePattern();
    TestFalse(TEXT("Target death cancels before the strike timer deals damage"), Enemy->bPatternActive);
    TestFalse(TEXT("Target death removes remaining timers"), World->GetTimerManager().TimerExists(Ability->PatternTimer));

    Target->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 1000.f);
    Row.Pattern = EPGAttackPattern::Sweep;
    Start();
    const FVector LockedForward = Ability->PatternForward;
    Ability->PatternStartedAt = World->GetTimeSeconds() - .5;
    Player->SetActorLocation(FVector(-150,0,90));
    Ability->UpdatePattern();
    TestTrue(TEXT("Aim stays locked when the target moves behind the attacker"), Ability->PatternForward.Equals(LockedForward));
    Ability->StrikeElitePattern();
    TestEqual(TEXT("Moving behind a locked sweep avoids the hit"), Target->GetHealth(), 1000.f);
    Source->CancelAbilityHandle(Handle);
    // Different responses to different shapes: approach the ring, sidestep the thrust.
    Row.Pattern = EPGAttackPattern::RingBurst; Row.InnerSafeRadius = 180; Row.TelegraphRadius = 450;
    Enemy->SetActorLocation(FVector(0,0,90)); Player->SetActorLocation(FVector(120,0,90));
    Start(); Ability->StrikeElitePattern();
    TestEqual(TEXT("Moving into the ring's safe center avoids damage"), Target->GetHealth(), 1000.f);
    Source->CancelAbilityHandle(Handle);
    Player->SetActorLocation(FVector(300,0,90));
    Start(); Ability->StrikeElitePattern();
    TestEqual(TEXT("Remaining in the visible ring receives exactly one hit"), Target->GetHealth(), 900.f);
    Source->CancelAbilityHandle(Handle);
    Row.Pattern = EPGAttackPattern::Thrust; Row.TravelDistance = 500; Row.LineHalfWidth = 45;
    Target->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 1000.f);
    Start(); Player->SetActorLocation(FVector(300,100,90)); Ability->StrikeElitePattern();
    TestEqual(TEXT("Sidestepping the locked thrust avoids damage"), Target->GetHealth(), 1000.f);
    Source->CancelAbilityHandle(Handle);
    Player->SetActorLocation(FVector(400,0,90));
    Start(); Ability->StrikeElitePattern();
    TestEqual(TEXT("Thrust reaches beyond melee sweep range"), Target->GetHealth(), 900.f);
    Source->CancelAbilityHandle(Handle);
    Row.Pattern = EPGAttackPattern::AimedProjectile; Row.ProjectileCount = 3; Row.ProjectileSpreadHalfAngle = 24;
    Target->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 1000.f);
    Start(); Ability->StrikeElitePattern();
    TestEqual(TEXT("Fan spawns its authored number of bolts"), Ability->ActiveProjectiles.Num(), 3);
    const auto Bolts = Ability->ActiveProjectiles;
    for (int32 Index = 0; Index < FMath::Min(2, Bolts.Num()); ++Index)
        if (auto* Bolt = Cast<APGPatternProjectile>(Bolts[Index].Get()))
            Bolt->OnProjectileOverlapped(nullptr, Player, nullptr, 0, false, FHitResult());
    TestEqual(TEXT("Overlapping fan bolts cannot shotgun one target"), Target->GetHealth(), 900.f);
    Source->CancelAbilityHandle(Handle);
    for (const auto& Bolt : Bolts)
        TestTrue(TEXT("Cancel removes every remaining fan projectile"), !Bolt.IsValid() || Bolt->IsActorBeingDestroyed());
    Row.Pattern = EPGAttackPattern::Sweep;
    Start();
    Source->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 0.f);
    Ability->UpdatePattern();
    TestFalse(TEXT("Source death cancels its sequence"), Enemy->bPatternActive);
    TestFalse(TEXT("Source death clears delayed hits"), World->GetTimerManager().TimerExists(Ability->PatternTimer));
    TestEqual(TEXT("Dead attacker never delivers the queued hit"), Target->GetHealth(), 900.f);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
