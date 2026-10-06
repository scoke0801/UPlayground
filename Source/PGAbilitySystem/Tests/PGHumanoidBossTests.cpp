#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGAbilitySystem/Abilities/Combat/PGEnemyAbilityAttack.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "AIController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGHumanoidBossDataTest, "PG.Content.HumanoidBossData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGHumanoidBossDataTest::RunTest(const FString&)
{
    auto* Profile = NewObject<UPGEnemyAttackProfile>();
    FPGEnemyAttackContact A, B; A.Time = .85f; B.Time = 1.75f; B.MotionStart = .95f;
    Profile->Contacts = {A, B};
    TestTrue(TEXT("Ordered contacts accepted"), Profile->IsValid());
    Profile->Contacts[1].Time = .5f;
    TestFalse(TEXT("Unordered contacts rejected"), Profile->IsValid());
    Profile->Contacts = {A}; Profile->bGuardCounter = true;
    TestTrue(TEXT("Bounded guard accepted"), Profile->IsValid());
    Profile->CounterTelegraphSeconds = .1f;
    TestFalse(TEXT("Unreadable counter rejected"), Profile->IsValid());
    Profile->CounterTelegraphSeconds = .65f; Profile->Contacts[0].DamageMultiplier = NAN;
    TestFalse(TEXT("Nonfinite damage rejected before commit"), Profile->IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGHumanoidBossLifecycleTest, "PG.Content.HumanoidBossLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGHumanoidBossLifecycleTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>(); auto* Player = World->SpawnActor<APGCharacterPlayer>();
    auto* PC = World->SpawnActor<APGPlayerController>(); PC->Possess(Player);
    auto* AI = World->SpawnActor<AAIController>(); AI->SetGenericTeamId(FGenericTeamId(1)); AI->Possess(Enemy);
    auto* Source = Enemy->GetPGAbilitySystemComponent(); auto* Target = Player->GetPGAbilitySystemComponent();
    Source->InitAbilityActorInfo(Enemy, Enemy); Target->InitAbilityActorInfo(Player, Player);
    Source->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Attack,100}});
    Target->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Defense,0}});
    Enemy->SetActorLocation(FVector(0,0,90)); Player->SetActorLocation(FVector(150,0,90));
    Player->GetCharacterMovement()->DisableMovement();
    const auto Handle = Source->GiveAbility(FGameplayAbilitySpec(UPGEnemyAbilityAttack::StaticClass(), 1));
    auto* Ability = Cast<UPGEnemyAbilityAttack>(Source->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    auto* Profile = NewObject<UPGEnemyAttackProfile>();
    FPGEnemyAttackContact A, B, C;
    A.Time = .85f; A.DamageMultiplier = .65f;
    B.MotionStart = .95f; B.Time = 1.75f; B.DamageMultiplier = .75f;
    C.MotionStart = 1.95f; C.Time = 2.95f; C.DamageMultiplier = 1.f;
    Profile->Contacts = {A,B}; Profile->PhaseTwoContacts = {A,B,C};
    FPGSkillDataRow Row; Row.Pattern = EPGAttackPattern::Sweep; Row.TelegraphDuration = .85f; Row.EnemyProfile = Profile;
    const auto Start = [&]()
    {
        Ability->PreActivate(Handle, Source->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), nullptr);
        Ability->UPGGameplayAbility::ActivateAbility(Handle, Source->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), nullptr);
        Ability->BeginElitePattern(Row); Ability->PatternTarget = Player; Ability->UpdateAim();
    };
    const auto Contact = [&](float At)
    { Ability->PatternStartedAt = World->GetTimeSeconds() - At; Ability->StrikeProfileContact(); };
    Start(); Contact(.85f);
    TestEqual(TEXT("First contact uses its damage budget"), Target->GetHealth(), 935.f);
    TestFalse(TEXT("No recovery between contacts"), Source->IsRecoveryExposed());
    Ability->StrikeProfileContact();
    TestEqual(TEXT("Duplicate/early callback cannot hit the next contact"), Target->GetHealth(), 935.f);
    Player->SetActorLocation(FVector(-150,0,90)); Contact(1.75f);
    TestEqual(TEXT("Escaping behind the locked combo avoids the second hit"), Target->GetHealth(), 935.f);
    TestTrue(TEXT("Recovery starts only after the final contact"), Source->IsRecoveryExposed());
    Source->CancelAbilityHandle(Handle);
    Player->SetActorLocation(FVector(150,0,90)); Target->RestoreHealth(1000.f);
    Start(); Contact(3.f); Ability->StrikeProfileContact();
    TestEqual(TEXT("A hitch cannot compress the next contact into an immediate hit"), Target->GetHealth(), 935.f);
    Source->CancelAbilityHandle(Handle);
    Enemy->BossPhase = 2; Player->SetActorLocation(FVector(150,0,90)); Target->RestoreHealth(1000.f);
    Start(); Contact(.85f); Contact(1.75f);
    TestEqual(TEXT("Phase two preserves first two budgets"), Target->GetHealth(), 860.f);
    TestFalse(TEXT("Phase two waits for its third hit"), Source->IsRecoveryExposed());
    Contact(2.95f); Ability->StrikeProfileContact();
    TestEqual(TEXT("Three contacts hit once each"), Target->GetHealth(), 760.f);
    Source->CancelAbilityHandle(Handle);
    Start(); Contact(.85f); Source->CancelAbilityHandle(Handle);
    const float CancelHealth = Target->GetHealth(); Contact(2.95f);
    TestEqual(TEXT("Cancellation prevents queued combo damage"), Target->GetHealth(), CancelHealth);
    TestFalse(TEXT("Cancellation clears timer"), World->GetTimerManager().TimerExists(Ability->PatternTimer));
    Profile->Contacts = {A}; Profile->PhaseTwoContacts.Reset(); Profile->bGuardCounter = true;
    Enemy->BossPhase = 1; Start();
    TestFalse(TEXT("Guard start has no reduction"), Enemy->bGuarding);
    Ability->BeginGuardHold();
    TestTrue(TEXT("Only hold enables boss guard"), Enemy->bGuarding && Enemy->bBossPatternGuard);
    Source->ReceiveProcDamage(Target, 10.f);
    TestFalse(TEXT("Proc damage cannot trigger a guard success"), Ability->bGuardSucceeded);
    Source->OnConfirmedGuardHit.Broadcast(Player);
    Source->OnConfirmedGuardHit.Broadcast(Player);
    TestTrue(TEXT("Confirmed guard releases defense once"), Ability->bGuardSucceeded && !Enemy->bGuarding);
    Ability->BeginCounter();
    const float BeforeCounter = Target->GetHealth(); Ability->StrikeProfileContact();
    TestEqual(TEXT("Counter cannot hit before its warning expires"), Target->GetHealth(), BeforeCounter);
    Ability->CounterStartedAt = World->GetTimeSeconds() - .65;
    Ability->StrikeProfileContact(); Ability->StrikeProfileContact();
    TestEqual(TEXT("Counter hits only once"), Target->GetHealth(), BeforeCounter - 65.f);
    Source->CancelAbilityHandle(Handle);
    Start(); Ability->BeginGuardHold(); Ability->FailGuard(); Ability->BeginRecovery();
    TestTrue(TEXT("Unattacked guard ends with exposure"), Source->IsRecoveryExposed() && !Enemy->bGuarding);
    Source->CancelAbilityHandle(Handle);
    TestFalse(TEXT("Cancellation releases guard opt-in"), Enemy->bBossPatternGuard);
    TestFalse(TEXT("Cancellation removes guard callback"), Source->OnConfirmedGuardHit.IsBound());
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}
#endif
