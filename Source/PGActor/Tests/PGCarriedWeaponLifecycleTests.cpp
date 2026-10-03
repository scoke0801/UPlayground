#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGActor/Components/Combat/PGPawnCombatComponent.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCarriedWeaponLifecycleTest, "PG.Combat.CarriedWeaponOwnerCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCarriedWeaponLifecycleTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* Owner = World->SpawnActor<APGCharacterBase>();
    auto* Combat = NewObject<UPGPawnCombatComponent>(Owner);
    Owner->AddInstanceComponent(Combat);
    Combat->RegisterComponent();
    Owner->DispatchBeginPlay();
    auto* Sword = World->SpawnActor<APGWeaponBase>();
    auto* Bow = World->SpawnActor<APGWeaponBase>();
    auto* UnrelatedWeapon = World->SpawnActor<APGWeaponBase>();
    Combat->RegisterSpawnedWeapon(PGGamePlayTags::Weapon_Sword, Sword);
    Combat->RegisterSpawnedWeapon(PGGamePlayTags::Weapon_Bow, Bow);
    Sword->Destroy(); // Normal death may already have destroyed one weapon.
    Owner->Destroy(); // Stage cleanup bypasses death and dissolve callbacks.
    TestFalse(TEXT("Direct pawn removal destroys remaining registered weapons"), IsValid(Bow));
    TestFalse(TEXT("Already destroyed weapons remain destroyed"), IsValid(Sword));
    TestNull(TEXT("Destroyed owner's carried map no longer retains weapons"), Combat->GetCharacterCarriedWeaponByTag(PGGamePlayTags::Weapon_Bow));
    TestTrue(TEXT("Other owners' weapons are untouched"), IsValid(UnrelatedWeapon));
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
