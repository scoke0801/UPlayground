#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGRoguelikeCombatTest, "PG.Roguelike.Combat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGRoguelikeCombatTest::RunTest(const FString& Parameters)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* Player = World->SpawnActor<APGCharacterPlayer>();
    auto* Enemy = World->SpawnActor<APGCharacterEnemy>();
    auto* Source = Player->GetPGAbilitySystemComponent();
    auto* Target = Enemy->GetPGAbilitySystemComponent();
    Source->InitAbilityActorInfo(Player,Player); Target->InitAbilityActorInfo(Enemy,Enemy);
    Source->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Attack,100}});
    Target->InitializeCombatStats({{EPGStatType::Health,10000},{EPGStatType::Defense,0}});
    Source->SetCombatPerks({{EPGCombatPerk::Bleed,10},{EPGCombatPerk::Frenzy,5}});
    EPGDamageType Type;
    Target->ReceiveCombatHit(Source,Type);
    TestEqual(TEXT("Direct attack seeds bleed"),Target->BleedRemaining,6);
    const float BeforeTick=Target->GetHealth();
    Target->TickBleed();
    TestEqual(TEXT("Bleed deals actual GAS damage"), BeforeTick-Target->GetHealth(),10.f);
    TestEqual(TEXT("Secondary damage never adds frenzy stacks"),Source->FrenzyStacks,1);
    TestTrue(TEXT("Direct hits increase attack animation rate"),Source->GetFrenzyRate()>1.f);
    for(int32 I=0;I<20;++I) Target->ReceiveCombatHit(Source,Type);
    TestEqual(TEXT("Bleed stacks bounded"),Target->BleedStacks,5);
    TestEqual(TEXT("Frenzy stacks bounded"),Source->FrenzyStacks,10);
    const int32 BeforeProcStacks=Target->BleedStacks;
    const float BeforeProc=Target->GetHealth();
    TestEqual(TEXT("Shock secondary damage applies once"),Target->ReceiveProcDamage(Source,125.f),125.f);
    TestEqual(TEXT("Secondary damage preserves bleed stacks"),Target->BleedStacks,BeforeProcStacks);
    TestEqual(TEXT("Secondary damage is exact"),BeforeProc-Target->GetHealth(),125.f);
    Source->SetCombatPerks({{EPGCombatPerk::Bleed,10},{EPGCombatPerk::BleedBurst,100}});
    Source->SetHeavySkill(true);
    const float BeforeBurst=Target->GetHealth();
    Target->ReceiveCombatHit(Source,Type);
    TestTrue(TEXT("Active strike detonates stored bleed"),BeforeBurst-Target->GetHealth()>100.f);
    Source->SetHeavySkill(false);
    Source->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),0);
    const float BeforeDeathTick=Target->GetHealth(); Target->TickBleed();
    TestEqual(TEXT("Dead source stops DOT"),Target->GetHealth(),BeforeDeathTick);
    TestFalse(TEXT("DOT timer cleaned"),World->GetTimerManager().IsTimerActive(Target->BleedTimer));
    TestEqual(TEXT("Dead source cannot heal or proc"),Target->ReceiveProcDamage(Source,100),0.f);
    TestEqual(TEXT("Death clears frenzy rate"),Source->GetFrenzyRate(),1.f);
    World->DestroyWorld(false); return true;
}
#endif
