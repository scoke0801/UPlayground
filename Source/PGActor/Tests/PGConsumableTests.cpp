#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGConsumableComponent.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGData/DataAsset/Progression/PGConsumableData.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPGConsumableLifecycleTest,"PG.Consumables.HealingAndLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPGConsumableLifecycleTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Player=World->SpawnActor<APGCharacterPlayer>();
    auto* PC=World->SpawnActor<APlayerController>();
    PC->Possess(Player);
    auto* ASC=Player->GetPGAbilitySystemComponent();
    ASC->InitializeCombatStats({{EPGStatType::Health,1000},{EPGStatType::Attack,100}});
    auto* Potion=Player->GetConsumableComponent();
    auto* Data=NewObject<UPGConsumableData>();
    Potion->Initialize(Data);
    auto* Stage=World->SpawnActor<APGStageManager>();
    Stage->CurrentStageState=EPGStageState::InProgress;
    Potion->BeginStage(Stage);
    auto SetHealth=[ASC](float Value){ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),Value);};
    TestEqual(TEXT("Starts with three charges"),Potion->GetState().Count,3);
    TestFalse(TEXT("Full health never consumes"),Potion->TryUse());
    TestEqual(TEXT("Rejected use has no cooldown"),Potion->GetRemainingCooldown(),0.f);
    SetHealth(250);
    bool Reentered=false;
    auto Handle=ASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetCurrentHealthAttribute()).AddLambda(
        [&](const FOnAttributeChangeData&){Reentered=Potion->TryUse();});
    TestTrue(TEXT("Injured player can heal"),Potion->TryUse());
    ASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetCurrentHealthAttribute()).Remove(Handle);
    TestFalse(TEXT("GAS health callbacks cannot recursively consume"),Reentered);
    TestEqual(TEXT("Forty percent of maximum is immediate"),ASC->GetHealth(),650.f);
    TestEqual(TEXT("Successful use costs exactly one"),Potion->GetState().Count,2);
    TestEqual(TEXT("Eight seconds begin on success"),Potion->GetRemainingCooldown(),8.f);
    TestFalse(TEXT("Repeated press while cooling down rejects"),Potion->TryUse());
    Potion->Initialize(Data);
    TestEqual(TEXT("Reinitialization does not replenish"),Potion->GetState().Count,2);
    const float BeforePause=Potion->GetRemainingCooldown();
    World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
    World->Tick(LEVELTICK_All,.2f);
    TestEqual(TEXT("Pause freezes potion cooldown"),Potion->GetRemainingCooldown(),BeforePause);
    TestFalse(TEXT("Pause rejects use"),Potion->TryUse());
    World->GetWorldSettings()->SetPauserPlayerState(nullptr);
    // World settings clamp oversized frame deltas; advance through realistic frames.
    for (int32 Frame=0; Frame<81; ++Frame) World->Tick(LEVELTICK_TimeOnly,.1f);
    TestEqual(TEXT("Game time releases cooldown"),Potion->GetRemainingCooldown(),0.f);
    SetHealth(850);
    Player->SetIsCanControl(false);
    TestTrue(TEXT("Hit control lock permits emergency healing"),Potion->TryUse());
    TestFalse(TEXT("Healing does not release hit control lock"),Player->GetIsCacControl());
    Player->SetIsCanControl(true);
    TestEqual(TEXT("Overheal clamps at maximum"),ASC->GetHealth(),1000.f);
    TestEqual(TEXT("Partial heal still costs one"),Potion->GetState().Count,1);
    Stage->CurrentStageState=EPGStageState::WaveIntermission;
    SetHealth(300);
    const float BeforeWave=Potion->GetRemainingCooldown();
    Potion->CompleteStage(Stage);
    TestEqual(TEXT("Wave intermission cannot replenish"),Potion->GetState().Count,1);
    TestEqual(TEXT("Wave intermission keeps damage"),ASC->GetHealth(),300.f);
    TestEqual(TEXT("Wave intermission keeps cooldown"),Potion->GetRemainingCooldown(),BeforeWave);
    Stage->CurrentStageState=EPGStageState::BuildPhase;
    Potion->CompleteStage(Stage);
    TestEqual(TEXT("Clearing a stage refills capacity"),Potion->GetState().Count,3);
    TestEqual(TEXT("Clearing a stage restores health"),ASC->GetHealth(),1000.f);
    TestEqual(TEXT("Maintenance clears cooldown"),Potion->GetRemainingCooldown(),0.f);
    Potion->Charges=1; SetHealth(700);
    Potion->CompleteStage(Stage);
    TestEqual(TEXT("Repeated reward choices cannot replenish twice"),Potion->GetState().Count,1);
    TestEqual(TEXT("Repeated maintenance callback does not heal twice"),ASC->GetHealth(),700.f);
    TestFalse(TEXT("No potion use during preparation"),Potion->TryUse());
    ASC->SetNumericAttributeBase(UPGAtrributeSet::GetMaxHealthAttribute(),1200);
    Stage->CurrentStageId=2; Stage->CurrentStageState=EPGStageState::InProgress;
    Potion->BeginStage(Stage);
    TestEqual(TEXT("Next stage includes maximum health changes"),ASC->GetHealth(),1200.f);
    TestEqual(TEXT("Next encounter starts with authored capacity"),Potion->GetState().Count,3);
    SetHealth(100);
    Potion->Charges=0;
    TestFalse(TEXT("Zero stock cannot heal"),Potion->TryUse());
    TestEqual(TEXT("Zero stock leaves health unchanged"),ASC->GetHealth(),100.f);
    Potion->Charges=3;
    PC->SetIgnoreMoveInput(true);
    TestFalse(TEXT("Modal input lock rejects healing"),Potion->TryUse());
    PC->ResetIgnoreMoveInput();
    SetHealth(0);
    TestFalse(TEXT("Cannot resurrect"),Potion->TryUse());
    TestEqual(TEXT("Death cannot consume charges"),Potion->GetState().Count,3);
    Potion->CompleteStage(Stage);
    TestEqual(TEXT("Stage events cannot resurrect"),ASC->GetHealth(),0.f);
    Data->HealFraction=std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Nonfinite healing rejected by data validation"),Data->IsValidDefinition());
    Data->HealFraction=.4f;
    Data->Capacity=0;
    TestFalse(TEXT("Zero capacity rejected by data validation"),Data->IsValidDefinition());
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
#endif
