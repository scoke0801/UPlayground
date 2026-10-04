#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGPawnCombatComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGPlayerMeleeProfileTest, "PG.Combat.PlayerMeleeProfiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGPlayerMeleeProfileTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* Player = World->SpawnActor<APGCharacterPlayer>();
    // A late animation callback after equipment cleanup must be a safe no-op.
    Player->GetCombatComponent()->ToggleWeaponCollision(false, EPGToggleDamageType::CurrentEquippedWeapon);
    Player->GetCombatComponent()->ToggleWeaponCollision(true, EPGToggleDamageType::CurrentEquippedWeapon);
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>();
    auto* Source = Player->GetPGAbilitySystemComponent();
    auto* Target = Enemy->GetPGAbilitySystemComponent();
    Source->InitAbilityActorInfo(Player, Player); Target->InitAbilityActorInfo(Enemy, Enemy);
    Source->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Attack,100}});
    Target->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Defense,100}});
    Source->ApplyPlayerMeleeHit(Enemy, 1.5f, true);
    TestEqual(TEXT("Finisher multiplier passes through defense and GAS health"), Target->GetHealth(), 925.f);
    TestFalse(TEXT("Heavy feedback state does not leak"), Player->bPerformingHeavyAttack);
    EPGDamageType Type;
    TestEqual(TEXT("Subsequent projectile/legacy damage is unscaled"), Target->ReceiveCombatHit(Source, Type), 50.f);
    Source->ApplyPlayerMeleeHit(Enemy, -1.f, false);
    TestEqual(TEXT("Invalid data cannot deal damage"), Target->GetHealth(), 875.f);
    Source->ApplyPlayerMeleeHit(Enemy, .7f, false);
    TestEqual(TEXT("Fast multi-hit strike uses its own coefficient"), Target->GetHealth(), 840.f);
    World->DestroyWorld(false);
    return true;
}
#endif
