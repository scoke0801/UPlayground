#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGAbilitySystem/Combat/PGCombatMath.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Tag/PGGamePlayStatusTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCombatMathTest, "PG.Combat.DamageBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCombatMathTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Unarmored"), PGCombatMath::Damage(100, 0, false, 0, 100, 1.5f, 1), 100.f);
    TestEqual(TEXT("Defense reduction"), PGCombatMath::Damage(100, 100, false, 0, 100, 1.5f, 1), 50.f);
    TestEqual(TEXT("Critical bonus"), PGCombatMath::Damage(100, 100, true, 50, 100, 1.5f, 1), 100.f);
    TestEqual(TEXT("Negative defense is clamped"), PGCombatMath::Damage(100, -100, false, 0, 100, 1.5f, 1), 100.f);
    TestEqual(TEXT("Minimum damage"), PGCombatMath::Damage(0, 1000, false, 0, 100, 1.5f, 1), 1.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCombatIntegrationTest, "PG.Combat.AttributesEquipmentAndHealing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCombatIntegrationTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto MakeASC = [World]()
    {
        AActor* Actor = World->SpawnActor<AActor>();
        UPGAbilitySystemComponent* ASC = NewObject<UPGAbilitySystemComponent>(Actor);
        Actor->AddInstanceComponent(ASC);
        ASC->RegisterComponent();
        ASC->InitAbilityActorInfo(Actor, Actor);
        return ASC;
    };
    UPGAbilitySystemComponent* Source = MakeASC();
    UPGAbilitySystemComponent* Target = MakeASC();
    Source->InitializeCombatStats({{EPGStatType::Attack, 20}, {EPGStatType::Health, 100}});
    Target->InitializeCombatStats({{EPGStatType::Defense, 100}, {EPGStatType::Health, 100}});
    EPGDamageType Type;
    TestEqual(TEXT("Damage applied without a hit reaction ability"), Target->ReceiveCombatHit(Source, Type), 10.f);
    TestEqual(TEXT("Health is GAS health"), Target->GetHealth(), 90.f);
    TestEqual(TEXT("Heal clamps to maximum"), Target->RestoreHealth(500.f), 10.f);
    TestEqual(TEXT("Negative heal rejected"), Target->RestoreHealth(-10.f), 0.f);
    TestEqual(TEXT("Missing source rejected"), Target->ReceiveCombatHit(nullptr, Type), 0.f);
    for (int32 Index = 0; Index < 100; ++Index)
    {
        Source->SetEquipmentBonuses({{EPGStatType::Attack, 10}});
        TestEqual(TEXT("Equipment applied once"), Source->GetCombatStat(EPGStatType::Attack), 30.f);
        Source->SetEquipmentBonuses({});
    }
    TestEqual(TEXT("Equipment removed without drift"), Source->GetCombatStat(EPGStatType::Attack), 20.f);
    for (int32 Index = 0; Index < 100; ++Index)
    {
        Source->SetEquipmentBonuses({{EPGStatType::Attack, 10}});
        Source->SetProfileBonuses({{EPGStatType::Attack, 30}});
        TestEqual(TEXT("Owned equipment coexists with actor weapon"), Source->GetCombatStat(EPGStatType::Attack), 60.f);
        Source->SetProfileBonuses({});
        TestEqual(TEXT("Owned equipment removal preserves actor weapon"), Source->GetCombatStat(EPGStatType::Attack), 30.f);
        Source->SetEquipmentBonuses({});
    }
    TestTrue(TEXT("Stat reward applied"), Source->ApplyStatBonus(EPGStatType::Attack, 5.f));
    TestEqual(TEXT("Reward persists"), Source->GetCombatStat(EPGStatType::Attack), 25.f);
    Target->SetNumericAttributeBase(UPGAtrributeSet::GetMaxHealthAttribute(), 0.f);
    TestEqual(TEXT("Zero max clamps current"), Target->GetHealth(), 0.f);
    TestEqual(TEXT("No resurrection through healing"), Target->RestoreHealth(100.f), 0.f);
    TestEqual(TEXT("Dead targets reject damage"), Target->ReceiveCombatHit(Source, Type), 0.f);
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGInputBufferTest, "PG.Combat.InputBuffer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGInputBufferTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    AActor* Owner = World->SpawnActor<AActor>();
    UPGAbilitySystemComponent* ASC = NewObject<UPGAbilitySystemComponent>(Owner);
    Owner->AddInstanceComponent(ASC);
    ASC->RegisterComponent();
    ASC->InitAbilityActorInfo(Owner, Owner);
    ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Normal);
    TestTrue(TEXT("Unavailable input is buffered"), ASC->BufferedInput.IsValid());
    FGameplayAbilitySpec Spec(UGameplayAbility::StaticClass());
    Spec.GetDynamicSpecSourceTags().AddTag(PGGamePlayTags::InputTag_Skill_Normal);
    const auto Handle = ASC->GiveAbility(Spec);
    ASC->RetryBufferedInput();
    TestTrue(TEXT("Ready ability activates inside window"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
    TestFalse(TEXT("Successful input consumed"), ASC->BufferedInput.IsValid());
    ASC->CancelAbilityHandle(Handle);
    ASC->BufferedInput = PGGamePlayTags::InputTag_Skill_Normal;
    ASC->BufferExpiresAt = FPlatformTime::Seconds() - 1.;
    ASC->RetryBufferedInput();
    TestFalse(TEXT("Expired input is discarded"), ASC->BufferedInput.IsValid());
    TestFalse(TEXT("Expired input does not activate"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
    ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_MustBeHeld_Block);
    ASC->OnAbilityInputReleased(PGGamePlayTags::InputTag_MustBeHeld_Block);
    TestFalse(TEXT("Released held input is discarded"), ASC->BufferedInput.IsValid());
    ASC->OnAbilityInputPressed(PGGamePlayTags::InputTag_Skill_Slot1);
    ASC->AddLooseGameplayTag(PGGamePlayTags::Shared_Status_Dead);
    ASC->RetryBufferedInput();
    TestFalse(TEXT("Death discards buffered input"), ASC->BufferedInput.IsValid());
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGCombatPerkTest, "PG.CombatCycle.PerksAndRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPGCombatPerkTest::RunTest(const FString& Parameters)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto MakeASC = [World]()
    {
        auto* Actor = World->SpawnActor<AActor>();
        auto* ASC = NewObject<UPGAbilitySystemComponent>(Actor);
        Actor->AddInstanceComponent(ASC); ASC->RegisterComponent(); ASC->InitAbilityActorInfo(Actor, Actor);
        ASC->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Attack,100}});
        return ASC;
    };
    auto* Source = MakeASC(); auto* Target = MakeASC(); EPGDamageType Type;
    Source->SetCombatPerks({{EPGCombatPerk::LifeSteal,10},{EPGCombatPerk::Execution,50},{EPGCombatPerk::Counter,65}});
    Source->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 500);
    TestEqual(TEXT("Healthy target normal damage"), Target->ReceiveCombatHit(Source, Type), 100.f);
    TestEqual(TEXT("Life steal uses applied damage"), Source->GetHealth(), 510.f);
    Target->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 350);
    TestEqual(TEXT("Execution includes exact threshold"), Target->ReceiveCombatHit(Source, Type), 150.f);
    Target->RestoreHealth(1000);
    Target->OpenRecoveryWindow(1.f, .35f);
    TestEqual(TEXT("Base vulnerability and counter combine additively"), Target->ReceiveCombatHit(Source, Type), 200.f);
    Target->CloseRecoveryWindow();
    TestEqual(TEXT("Cancellation removes vulnerability"), Target->ReceiveCombatHit(Source, Type), 100.f);
    Target->OpenRecoveryWindow(.1f, .35f); World->Tick(LEVELTICK_All, .2f);
    TestFalse(TEXT("Recovery expires without a finishing callback"), Target->IsRecoveryExposed());
    Target->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 5);
    const float Before = Source->GetHealth();
    TestEqual(TEXT("Overkill reports remaining health"), Target->ReceiveCombatHit(Source, Type), 5.f);
    TestEqual(TEXT("Overkill cannot amplify life steal"), Source->GetHealth() - Before, .5f);
    TestEqual(TEXT("Dead target cannot be farmed"), Target->ReceiveCombatHit(Source, Type), 0.f);
    Source->SetCombatPerks({});
    TestEqual(TEXT("New run clears modifiers"), Source->GetPerkPercent(EPGCombatPerk::LifeSteal), 0);
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return true;
}
#endif
