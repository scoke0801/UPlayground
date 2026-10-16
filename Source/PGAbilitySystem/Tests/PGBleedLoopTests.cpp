#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGBleedLoopTest,"PG.HackSlash.BleedBurstPropagation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPGBleedLoopTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* Player=World->SpawnActor<APGCharacterPlayer>();
    auto* ASC=Player->GetPGAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player,Player);
    ASC->InitializeCombatStats({{EPGStatType::Health,10000},{EPGStatType::Attack,100},{EPGStatType::CriticalRate,0}});
    const auto Spawn=[&](FVector Location,int32 HP)
    {
        auto* Enemy=World->SpawnActor<APGCharacterEnemy>();
        auto* Target=Enemy->GetPGAbilitySystemComponent();
        Target->InitAbilityActorInfo(Enemy,Enemy);
        Target->InitializeCombatStats({{EPGStatType::Health,HP},{EPGStatType::Defense,0}});
        Enemy->SetActorLocation(Location);
        Enemy->GetCapsuleComponent()->SetCollisionObjectType(ECC_GameTraceChannel1);
        Enemy->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        return Enemy;
    };
    auto* Victim=Spawn(FVector(150,0,96),250);
    auto* Neighbor=Spawn(FVector(150,150,96),10000);
    auto* Outside=Spawn(FVector(150,1000,96),10000);
    ASC->SetCombatPerks({{EPGCombatPerk::Bleed,10},{EPGCombatPerk::BleedBurst,100},{EPGCombatPerk::BleedSpread,100}});
    auto Cast=MakeShared<FPGSkillCastContext>(); Cast->Caster=Player; Cast->SkillID=112; Cast->Attack=100;
    FPGHitProcPolicy Policy; Policy.bShock=false; Policy.bFrenzy=false;
    ASC->ApplyPlayerProfileHit(Victim,Cast,0,1.f,false,Policy,0);
    Policy.bBleedBurst=true;
    ASC->ApplyPlayerProfileHit(Victim,Cast,1,1.f,false,Policy,0);
    TestEqual(TEXT("Prepared bleed burst finishes the survivor"),Victim->GetPGAbilitySystemComponent()->GetHealth(),0.f);
    TestEqual(TEXT("Spread does not recursively deal direct damage"),Neighbor->GetPGAbilitySystemComponent()->GetHealth(),10000.f);
    Policy.bBleed=false; Policy.bBleedBurst=false;
    ASC->ApplyPlayerProfileHit(Neighbor,Cast,2,.01f,false,Policy,0);
    TestEqual(TEXT("Burst kill carries consumed bleed to nearby target"),ASC->GetBuildCombatState().BleedStacks,1);
    ASC->ApplyPlayerProfileHit(Outside,Cast,2,.01f,false,Policy,0);
    TestEqual(TEXT("Propagation respects radius"),ASC->GetBuildCombatState().BleedStacks,0);
    ASC->ApplyPlayerProfileHit(Victim,Cast,1,1.f,false,Policy,0);
    TestEqual(TEXT("Dead target cannot receive repeated phase damage"),Victim->GetPGAbilitySystemComponent()->GetHealth(),0.f);
    auto* Survivor=Spawn(FVector(2000,0,96),10000);
    auto* Untouched=Spawn(FVector(2000,150,96),10000);
    Policy.bBleed=true; Policy.bBleedBurst=true;
    ASC->ApplyPlayerProfileHit(Survivor,Cast,3,1.f,false,Policy,0);
    TestTrue(TEXT("Nonlethal burst still deals damage"),Survivor->GetPGAbilitySystemComponent()->GetHealth()<9900.f);
    Policy.bBleed=false; Policy.bBleedBurst=false;
    ASC->ApplyPlayerProfileHit(Untouched,Cast,4,.01f,false,Policy,0);
    TestEqual(TEXT("Nonlethal burst cannot propagate"),ASC->GetBuildCombatState().BleedStacks,0);
    World->DestroyWorld(false);
    return true;
}
#endif
