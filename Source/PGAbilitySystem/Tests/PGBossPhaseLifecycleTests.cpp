#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PGAbilitySystem/Abilities/Combat/PGEnemyAbilityAttack.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGBossPhaseLifecycleTest, "PG.Content.BossPhaseLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGBossPhaseLifecycleTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
    const auto AdvanceTimers = [&](float Seconds)
    {
        ++GFrameCounter; World->GetTimerManager().Tick(0.f); // Promote newly pending timers.
        ++GFrameCounter; World->GetTimerManager().Tick(Seconds);
    };
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>();
    auto* Player = World->SpawnActor<APGCharacterPlayer>();
    auto* PC = World->SpawnActor<APGPlayerController>(); PC->Possess(Player);
    auto* AI = World->SpawnActor<AAIController>(); AI->SetGenericTeamId(FGenericTeamId(1)); AI->Possess(Enemy);
    auto* BossASC = Enemy->GetPGAbilitySystemComponent();
    auto* PlayerASC = Player->GetPGAbilitySystemComponent();
    BossASC->InitAbilityActorInfo(Enemy, Enemy); PlayerASC->InitAbilityActorInfo(Player, Player);
    BossASC->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Attack,100}});
    PlayerASC->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Defense,0}});
    Enemy->SetActorLocation(FVector(0,0,90)); Player->SetActorLocation(FVector(150,0,90));
    Player->GetCharacterMovement()->DisableMovement();
    const auto Handle = BossASC->GiveAbility(FGameplayAbilitySpec(UPGEnemyAbilityAttack::StaticClass(), 1));
    auto* Ability = Cast<UPGEnemyAbilityAttack>(BossASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!TestNotNull(TEXT("Boss pattern instance"), Ability)) { World->DestroyWorld(false); return false; }
    FPGEnemyDataRow BossData; BossData.Role = EPGEnemyRole::Boss;
    FPGSkillDataRow Pattern; Pattern.TelegraphDuration = 1.f; Pattern.TelegraphRadius = 200; Pattern.RecoveryDuration = .5f;
    int32 Transitions = 0;
    // An isolated world has no authored tables. Drive the real threshold handler from GAS's health delegate.
    const auto HealthDelegate = BossASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetCurrentHealthAttribute()).AddLambda(
        [&](const FOnAttributeChangeData&)
        {
            Enemy->OnHealthChanged();
            if (Enemy->TryBeginBossPhase(BossData)) ++Transitions;
        });
    const auto Start = [&]()
    {
        Ability->PreActivate(Handle, BossASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), nullptr);
        Ability->UPGGameplayAbility::ActivateAbility(Handle, BossASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), nullptr);
        Ability->BeginElitePattern(Pattern);
        Ability->PatternTarget = Player; Ability->UpdateAim();
    };
    for (const auto Kind : {EPGAttackPattern::Sweep, EPGAttackPattern::ChargeSlam, EPGAttackPattern::HazardSequence})
    {
        BossASC->RestoreHealth(1000.f);
        Enemy->BossPhase = 1;
        Pattern.Pattern = Kind;
        const int32 Before = Transitions;
        BossASC->ReceiveProcDamage(PlayerASC, 499.f);
        TestEqual(TEXT("Above threshold stays in phase one"), Enemy->BossPhase, 1);
        Start();
        if (Kind != EPGAttackPattern::Sweep) Ability->StrikeElitePattern(); // In-flight charge or pending hazard.
        BossASC->ReceiveProcDamage(PlayerASC, 250.f);
        TestEqual(TEXT("Large GAS hit transitions once"), Transitions, Before + 1);
        TestTrue(TEXT("Transition gate is active"), Enemy->IsBossTransitioning());
        TestFalse(TEXT("Transition cancels current attack"), Enemy->bPatternActive);
        TestFalse(TEXT("Transition removes delayed strike"), World->GetTimerManager().TimerExists(Ability->PatternTimer));
        TestFalse(TEXT("Transition removes charge update"), World->GetTimerManager().TimerExists(Ability->UpdateTimer));
        TestNull(TEXT("Transition removes telegraph"), Ability->Telegraph.Get());
        TestFalse(TEXT("Transition closes recovery modifier"), BossASC->IsRecoveryExposed());
        const double Until = Enemy->PhaseTransitionUntil;
        BossASC->ReceiveProcDamage(PlayerASC, 1.f);
        BossASC->RestoreHealth(700.f);
        BossASC->ReceiveProcDamage(PlayerASC, 600.f);
        TestEqual(TEXT("Healing and recrossing cannot retrigger"), Transitions, Before + 1);
        TestEqual(TEXT("Further hits do not extend transition"), Enemy->PhaseTransitionUntil, Until);
        BossASC->TryActivateAbility(Handle);
        TestFalse(TEXT("Direct GAS activation also respects transition"), BossASC->FindAbilitySpecFromHandle(Handle)->IsActive());
        const float HealthAfterCancel = PlayerASC->GetHealth();
        AdvanceTimers(3.f);
        TestEqual(TEXT("Canceled attack causes no later damage"), PlayerASC->GetHealth(), HealthAfterCancel);
        TestFalse(TEXT("Transition ends without invulnerability"), Enemy->IsBossTransitioning());
    }
    BossASC->RestoreHealth(1000.f);
    Enemy->BossPhase = 1;
    Pattern.Pattern = EPGAttackPattern::HazardSequence;
    Start(); Ability->StrikeElitePattern();
    const float BeforeDeath = PlayerASC->GetHealth();
    const int32 BeforeLethal = Transitions;
    BossASC->ReceiveProcDamage(PlayerASC, 10000.f);
    TestEqual(TEXT("Lethal threshold crossing skips transition"), Transitions, BeforeLethal);
    TestFalse(TEXT("Death cancels pending hazard"), Enemy->bPatternActive);
    AdvanceTimers(1.f);
    TestEqual(TEXT("Death cannot deliver delayed damage"), PlayerASC->GetHealth(), BeforeDeath);
    BossASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetCurrentHealthAttribute()).Remove(HealthDelegate);
    World->DestroyWorld(false);
    return true;
}
#endif
