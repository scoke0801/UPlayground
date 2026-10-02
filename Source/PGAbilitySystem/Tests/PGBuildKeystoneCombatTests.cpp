#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/DataAsset/Combat/PGCombatTuningData.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGBuildKeystoneCombatTest, "PG.Content.BuildKeystoneCombat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGBuildKeystoneCombatTest::RunTest(const FString& Parameters)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* Player = World->SpawnActor<APGCharacterPlayer>();
    auto* PC = World->SpawnActor<APlayerController>(); PC->Possess(Player);
    auto* Source = Player->GetPGAbilitySystemComponent();
    Source->InitAbilityActorInfo(Player,Player);
    Source->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Attack,100}});
    Source->CombatTuning = NewObject<UPGCombatTuningData>();
    auto* Handler = Player->GetSkillHandler();
    if (!TestNotNull(TEXT("Possession creates real skill handler"),Handler)) { World->DestroyWorld(false); return false; }
    Handler->AddSkill(EPGSkillSlot::SkillSlot_1,0);
    auto* Skill = Handler->GetSkillData(EPGSkillSlot::SkillSlot_1);
    Skill->CoolTime=10.f; Skill->LastSkillUsedTime=FPlatformTime::Seconds();
    TArray<UPGAbilitySystemComponent*> Targets;
    for (int32 I=0;I<3;++I)
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Enemy = World->SpawnActor<APGCharacterEnemy>(FVector(100,I*70,0),FRotator::ZeroRotator,Params);
        TestEqual(TEXT("Production enemy collision profile is used"),Enemy->GetCapsuleComponent()->GetCollisionObjectType(),ECC_GameTraceChannel1);
        Enemy->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        auto* Target = Enemy->GetPGAbilitySystemComponent(); Target->InitAbilityActorInfo(Enemy,Enemy);
        Target->InitializeCombatStats({{EPGStatType::Health,10000},{EPGStatType::Defense,100}});
        Targets.Add(Target);
    }
    Source->SetCombatPerks({{EPGCombatPerk::Bleed,12},{EPGCombatPerk::BleedBurst,100},{EPGCombatPerk::BleedRecast,1}});
    Source->BeginCombatSkill(EPGSkillSlot::SkillSlot_1);
    const double BeforeRefund = Skill->LastSkillUsedTime;
    EPGDamageType Type;
    for (int32 I=0;I<2;++I)
    {
        auto* Target=Targets[I];
        Target->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),100.f);
        Target->AddBleed(Source,100.f);
        Target->ReceiveCombatHit(Source,Type);
    }
    const double Refund = BeforeRefund-Skill->LastSkillUsedTime;
    TestTrue(TEXT("Two burst kills refund only once per active skill"),Refund>3.3 && Refund<3.6);
    TestTrue(TEXT("Refund has HUD feedback"),Source->GetBuildCombatState().bRefundProc);
    Source->BeginCombatSkill(EPGSkillSlot::SkillSlot_1);
    Targets[2]->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),1.f);
    Targets[2]->AddBleed(Source,100.f);
    const double BeforeDirectKill = Skill->LastSkillUsedTime;
    Targets[2]->ReceiveCombatHit(Source,Type);
    TestEqual(TEXT("Direct kill is not a burst kill"),Skill->LastSkillUsedTime,BeforeDirectKill);

    auto* Enemy = World->SpawnActor<APGCharacterEnemy>(FVector(140,0,0),FRotator::ZeroRotator);
    auto* Target=Enemy->GetPGAbilitySystemComponent(); Target->InitAbilityActorInfo(Enemy,Enemy);
    Target->InitializeCombatStats({{EPGStatType::Health,10000},{EPGStatType::Defense,100}});
    TestEqual(TEXT("Pulse includes production enemy channel"),Enemy->GetCapsuleComponent()->GetCollisionObjectType(),ECC_GameTraceChannel1);
    Enemy->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Source->SetCombatPerks({{EPGCombatPerk::Shockwave,40},{EPGCombatPerk::ShockFracture,1}});
    for (int32 I=0;I<2;++I) Target->ReceiveProcDamage(Source,10,EPGDamageCause::Shockwave);
    TestEqual(TEXT("Below threshold keeps defense"),Target->GetEffectiveDefense(),100.f);
    Target->ReceiveProcDamage(Source,10,EPGDamageCause::ShockEcho);
    TestEqual(TEXT("Echo reaches fracture threshold"),Target->GetEffectiveDefense(),70.f);
    TestEqual(TEXT("Secondary hit never seeds bleed"),Target->BleedStacks,0);
    Source->SetHeavySkill(false);
    const float Damage=Target->ReceiveCombatHit(Source,Type);
    TestTrue(TEXT("Fracture increases actual direct GAS damage"),Damage>58.f && Damage<59.f);
    TestTrue(TEXT("Target weakness is visible"),Source->GetBuildCombatState().WeaknessSeconds>0);
    Source->SetCombatPerks({});
    TestEqual(TEXT("Removing equipped effects immediately restores defense"),Target->GetEffectiveDefense(),100.f);
    Source->SetCombatPerks({{EPGCombatPerk::Shockwave,40},{EPGCombatPerk::ShockFracture,1}});
    TestEqual(TEXT("Re-equipping does not resurrect old weakness"),Target->GetEffectiveDefense(),100.f);
    for (int32 I=0;I<3;++I) Target->ReceiveProcDamage(Source,10,EPGDamageCause::Shockwave);
    Target->WeaknessUntil = World->GetTimeSeconds()-1;
    TestEqual(TEXT("Expired defense debuff restores defense"),Target->GetEffectiveDefense(),100.f);

    Source->SetCombatPerks({{EPGCombatPerk::Frenzy,5},{EPGCombatPerk::FrenzyAfterimage,1}});
    for(int32 I=0;I<10;++I) Target->ReceiveCombatHit(Source,Type);
    const float BeforeDodge=Target->GetHealth();
    Source->OnDodgeCommitted();
    TestEqual(TEXT("Dodge consumes all stacks"),Source->GetBuildCombatState().FrenzyStacks,0);
    TestEqual(TEXT("Afterimage deals one attack-scaled GAS hit"),BeforeDodge-Target->GetHealth(),150.f);
    Source->OnDodgeCommitted();
    TestEqual(TEXT("Duplicate dodge cannot repeat damage"),BeforeDodge-Target->GetHealth(),150.f);
    TestEqual(TEXT("Afterimage never builds frenzy"),Source->FrenzyStacks,0);
    TestTrue(TEXT("Afterimage has HUD feedback"),Source->GetBuildCombatState().bAfterimageProc);
    Source->SetCombatPerks({{EPGCombatPerk::Bleed,12},{EPGCombatPerk::BleedPotency,50}});
    Target->AddBleed(Source,100,true);
    Target->AddBleed(Source,100,true);
    TestEqual(TEXT("Repeated spread never multiplies snapshot by potency or stacks"),Target->BleedDamage,100.f);
    TestEqual(TEXT("Spread refresh does not synthesize direct-hit stacks"),Target->BleedStacks,1);
    Target->BleedRemaining=1;
    Target->TickBleed();
    TestEqual(TEXT("Final tick clears stacks immediately"),Target->BleedStacks,0);
    TestFalse(TEXT("Final tick cancels timer"),World->GetTimerManager().IsTimerActive(Target->BleedTimer));
    Target->AddBleed(Source,10);
    Source->SetCombatPerks({});
    Source->SetCombatPerks({{EPGCombatPerk::Bleed,12}});
    const float BeforeRemovedBleed=Target->GetHealth();
    Target->TickBleed();
    TestEqual(TEXT("Re-equipping does not resurrect old bleed"),Target->GetHealth(),BeforeRemovedBleed);
    Source->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),0);
    TestEqual(TEXT("Death clears visible transient state"),Source->GetBuildCombatState().FrenzyMaxStacks,0);
    Source->OnDodgeCommitted();
    TestEqual(TEXT("Dead source cannot trigger effects"),Target->GetHealth(),BeforeRemovedBleed);
    World->DestroyWorld(false);
    return true;
}
#endif
