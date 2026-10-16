#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "GameplayEffect.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGStatDetailsTest, "PG.Combat.StatDetailsPreview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGStatDetailsTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* Owner=World->SpawnActor<AActor>();
    auto* ASC=NewObject<UPGAbilitySystemComponent>(Owner);
    Owner->AddInstanceComponent(ASC); ASC->RegisterComponent(); ASC->InitAbilityActorInfo(Owner,Owner);
    TestFalse(TEXT("Uninitialized preview unavailable"),ASC->GetStatDetails(EPGStatType::Attack).bValid);
    TMap<EPGStatType,int32> Base, Weapon, Before, After;
    for (int32 I=1;I<int32(EPGStatType::Max);++I)
    {
        Base.Add(EPGStatType(I),100); Weapon.Add(EPGStatType(I),10);
        Before.Add(EPGStatType(I),30); After.Add(EPGStatType(I),60);
    }
    ASC->InitializeCombatStats(Base); ASC->SetEquipmentBonuses(Weapon); ASC->SetProfileBonuses(Before);
    ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),75.f);
    auto AddEffect=[&](EGameplayModOp::Type Op,float Amount)
    {
        auto* Effect=NewObject<UGameplayEffect>(); Effect->DurationPolicy=EGameplayEffectDurationType::Infinite;
        FGameplayModifierInfo Mod; Mod.Attribute=UPGAtrributeSet::GetAttackPowerAttribute();
        Mod.ModifierOp=Op; Mod.ModifierMagnitude=FScalableFloat(Amount); Effect->Modifiers.Add(Mod);
        return ASC->ApplyGameplayEffectToSelf(Effect,1.f,ASC->MakeEffectContext());
    };
    const auto Buff=AddEffect(EGameplayModOp::Multiplicitive,1.5f);
    const auto Flat=AddEffect(EGameplayModOp::Additive,7.5f);
    const auto Divide=AddEffect(EGameplayModOp::Division,2.f);
    int32 Changes=0;
    const auto Delegate=ASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetAttackPowerAttribute()).AddLambda([&](const FOnAttributeChangeData&){++Changes;});
    const int32 EffectCount=ASC->GetActiveEffects(FGameplayEffectQuery()).Num();
    TMap<EPGStatType,float> Expected;
    for (int32 I=1;I<int32(EPGStatType::Max);++I)
    {
        const auto Stat=EPGStatType(I);
        for (int32 Repeat=0;Repeat<20;++Repeat)
        {
            const auto Details=ASC->GetStatDetails(Stat,60.f);
            TestTrue(TEXT("Valid snapshot"),Details.bValid);
            TestEqual(TEXT("Base source"),Details.Base,100.f);
            TestEqual(TEXT("Weapon source distinct from profile"),Details.Weapon,10.f);
            TestEqual(TEXT("Profile source"),Details.Profile,30.f);
            TestTrue(TEXT("Sources reconcile final"),FMath::IsNearlyEqual(Details.Current,Details.Base+Details.Weapon+Details.Profile+Details.Other));
            Expected.Add(Stat,Details.Expected);
        }
    }
    TestEqual(TEXT("Preview emits no live attribute changes"),Changes,0);
    TestEqual(TEXT("Preview preserves health"),ASC->GetHealth(),75.f);
    TestEqual(TEXT("Preview preserves effect count"),ASC->GetActiveEffects(FGameplayEffectQuery()).Num(),EffectCount);
    ASC->SetProfileBonuses(After);
    for (const auto& Pair : Expected) TestEqual(TEXT("All seven previews match actual GAS replacement"),ASC->GetCombatStat(Pair.Key),Pair.Value);
    TestEqual(TEXT("Fractional multiply and divide preserved"),Expected.FindRef(EPGStatType::Attack),133.125f);
    const auto Override=AddEffect(EGameplayModOp::Override,42.f);
    TestEqual(TEXT("Override wins over replacement"),ASC->GetStatDetails(EPGStatType::Attack,999.f).Expected,42.f);
    ASC->RemoveActiveGameplayEffect(Override); ASC->RemoveActiveGameplayEffect(Buff);
    ASC->RemoveActiveGameplayEffect(Flat); ASC->RemoveActiveGameplayEffect(Divide);
    ASC->SetProfileBonuses({{EPGStatType::CriticalRate,15000}});
    TestEqual(TEXT("Capped current"),ASC->GetCombatStat(EPGStatType::CriticalRate),10000.f);
    TestEqual(TEXT("Cap preserved after partial decrease"),ASC->GetStatDetails(EPGStatType::CriticalRate,12000.f).Expected,10000.f);
    const float Uncapped=ASC->GetStatDetails(EPGStatType::CriticalRate,1000.f).Expected;
    ASC->SetProfileBonuses({{EPGStatType::CriticalRate,1000}});
    TestEqual(TEXT("Uncapped preview matches replacement"),ASC->GetCombatStat(EPGStatType::CriticalRate),Uncapped);
    TestEqual(TEXT("Negative replacement clamps"),ASC->GetStatDetails(EPGStatType::Attack,-1000.f).Expected,0.f);
    ASC->SetEquipmentBonuses({{EPGStatType::CriticalRate,17}});
    ASC->SetProfileBonuses({{EPGStatType::CriticalRate,68}});
    const auto Critical=UPGAtrributeSet::GetCriticalRateAttribute();
    ASC->SetNumericAttributeBase(Critical,0.f);
    const float CriticalBonus=ASC->GetCombatStat(EPGStatType::CriticalRate);
    TestEqual(TEXT("Zero base still includes equipment and profile critical"),CriticalBonus,85.f);
    ASC->SetNumericAttributeBase(Critical,-CriticalBonus);
    TestEqual(TEXT("Isolated critical fixture offsets bonuses"),ASC->GetCombatStat(EPGStatType::CriticalRate),0.f);
    TestEqual(TEXT("Fixture preserves equipment critical bonus"),ASC->GetStatDetails(EPGStatType::CriticalRate).Weapon,17.f);
    TestEqual(TEXT("Fixture preserves profile critical bonus"),ASC->GetStatDetails(EPGStatType::CriticalRate).Profile,68.f);
    ASC->SetNumericAttributeBase(Critical,10000.f-CriticalBonus);
    TestEqual(TEXT("Isolated guaranteed critical fixture"),ASC->GetCombatStat(EPGStatType::CriticalRate),10000.f);
    TestFalse(TEXT("Invalid stat unavailable"),ASC->GetStatDetails(EPGStatType::None).bValid);
    ASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetAttackPowerAttribute()).Remove(Delegate);
    World->DestroyWorld(false);
    return true;
}
#endif
