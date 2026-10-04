#include "PGCheatManager.h"

#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "NavigationSystem.h"
#include "UnrealClient.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "HAL/IConsoleManager.h"
#include "PGAI/PGRoleAIController.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Manager/PGStagePresentation.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataTemplate.h"
#include "PGUI/Manager/PGUIManager.h"

namespace
{
struct FPGSkillTrial
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<APGCharacterPlayer> Player;
    TWeakObjectPtr<UPGMessageManager> Messages;
    TArray<TWeakObjectPtr<APGCharacterEnemy>> Enemies;
    TArray<TPair<TWeakObjectPtr<UPGAbilitySystemComponent>, FDelegateHandle>> HealthHandles;
    FDelegateHandle SkillHandle;
    double Requested = FPlatformTime::Seconds();
    double Started = 0.;
    float Damage = 0.f, Taken = 0.f;
    int32 Kills = 0, Uses = 0;
    TSet<int32> Dead;
    bool bReady = false, bFinished = false, bNavigationRequested = false, bCaptured = false;
    bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("PGSkillScenarioSmoke"));
    FString Scenario, Variant, Build = TEXT("none");
    int32 Seed = 0;

    ~FPGSkillTrial()
    {
        for (const auto& Pair : HealthHandles)
            if (auto* ASC = Pair.Key.Get())
                ASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetCurrentHealthAttribute()).Remove(Pair.Value);
        if (Messages.IsValid()) Messages->UnregisterDelegate(EPGPlayerMessageType::UseSkill, SkillHandle);
    }

    bool Finish(const TCHAR* Outcome)
    {
        bFinished = true;
        const double Elapsed = bReady && World.IsValid() ? World->GetTimeSeconds() - Started : 0.;
        for (auto Weak : Enemies)
            if (auto* Enemy = Weak.Get())
            {
                if (auto* AI = Cast<APGRoleAIController>(Enemy->GetController())) AI->SetCombatThinkingEnabled(false);
                Enemy->GetPGAbilitySystemComponent()->CancelAbilities();
                Enemy->GetCharacterMovement()->StopMovementImmediately();
            }
        if (Player.IsValid())
        {
            Player->GetPGAbilitySystemComponent()->ClearBufferedInput();
            Player->GetPGAbilitySystemComponent()->CancelAbilities();
        }
        // Include cancellation/observation cleanup before the final trial boundary.
        UE_LOG(LogTemp, Display, TEXT("PGSkillTrial END outcome=%s seconds=%.3f damage=%.3f taken=%.3f kills=%d uses=%d smoke=%d"),
            Outcome, Elapsed, Damage, Taken, Kills, Uses, bSmoke);
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Yellow,
            FString::Printf(TEXT("%s: %s | %.1fs | damage %.0f / taken %.0f | kills %d"), *Scenario, Outcome, Elapsed, Damage, Taken, Kills));
        if (bSmoke || FParse::Param(FCommandLine::Get(), TEXT("PGSkillTrialExit"))) FPlatformMisc::RequestExit(false);
        return false;
    }
};
}
#endif

void UPGCheatManager::PGSkillScenario(FString Scenario, int32 Seed, FString Variant)
{
#if !UE_BUILD_SHIPPING
    FString ProfileName;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), ProfileName)
        || !ProfileName.StartsWith(TEXT("HackSlash_")) || Seed <= 0
        || !(Variant == TEXT("p0") || Variant == TEXT("baseline"))
        || !(Scenario == TEXT("P0-M10") || Scenario == TEXT("P0-M15") || Scenario == TEXT("P0-RING") || Scenario == TEXT("P0-E1")))
    {
        UE_LOG(LogTemp, Warning, TEXT("PGSkillTrial REJECT invalid arguments or non-test profile"));
        return;
    }
    auto* W = GetWorld();
    if (!W || W->GetWorldSettings()->ActorHasTag(TEXT("PGSkillTrialUsed")))
    {
        UE_LOG(LogTemp, Warning, TEXT("PGSkillTrial REJECT use a fresh process for each trial"));
        return;
    }
    W->GetWorldSettings()->Tags.Add(TEXT("PGSkillTrialUsed"));
    auto State = MakeShared<FPGSkillTrial>();
    if (auto* Observe = IConsoleManager::Get().FindConsoleVariable(TEXT("pg.Skill.Observe"))) Observe->Set(1, ECVF_SetByCode);
    State->World = W; State->Scenario = Scenario; State->Variant = Variant; State->Seed = Seed;
    FParse::Value(FCommandLine::Get(), TEXT("PGSkillTrialBuild="), State->Build);
    if (State->Build != TEXT("none") && State->Build != TEXT("bleed") && State->Build != TEXT("shock") && State->Build != TEXT("frenzy"))
    { State->Finish(TEXT("invalid_build")); return; }
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State](float)
    {
        auto* World = State->World.Get();
        if (!World) return State->Finish(TEXT("world_lost"));
        auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
        if (!State->bReady)
        {
            if (FPlatformTime::Seconds() - State->Requested > 45.) return State->Finish(TEXT("setup_timeout"));
            if (!Player || !Player->GetSkillHandler() || !Player->GetMesh()->GetAnimInstance()
                || FPlatformTime::Seconds() - State->Requested < 4.) return true;
            auto* Profile = UPGProfileSubsystem::Get(Player);
            auto* Tables = UPGDataTableManager::Get(Player);
            auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
            if (!Profile || !Tables || !Nav) return true;
            // RogueArena builds its dynamic navigation when a wave starts. This isolated
            // trial bypasses wave start, so request the same initialization explicitly.
            if (!State->bNavigationRequested)
            {
                for (TActorIterator<APGStageManager> It(World); It; ++It) It->PrepareNavigationForWave();
                State->bNavigationRequested = true;
                return true;
            }
            if (Nav->IsNavigationBuildInProgress()) return true;
            if (!Profile->ConfigureBuildScenario({})) return State->Finish(TEXT("profile_failed"));
            for (TActorIterator<APGStageManager> It(World); It; ++It)
            {
                It->CurrentStageState = EPGStageState::None;
                It->GetWorldTimerManager().ClearAllTimersForObject(*It);
                It->CloseRewardWindow(); It->ClearAllEnemies();
                It->MonsterSpawnQueue.Reset(); It->ActiveWaves.Reset();
                It->CurrentWaveIndex = INDEX_NONE; It->RemainingMonsters = 0;
                It->RewardToken.Invalidate();
            }
            if (State->Variant == TEXT("baseline"))
            {
                FString Path, Json;
                if (!FParse::Value(FCommandLine::Get(), TEXT("PGSkillBaseline="), Path)
                    || !FFileHelper::LoadFileToString(Json, *Path)) return State->Finish(TEXT("baseline_missing"));
                auto* Backup = NewObject<UDataTable>(); Backup->RowStruct = FPGSkillDataRow::StaticStruct();
                if (!Backup->CreateTableFromJSONString(Json).IsEmpty()) return State->Finish(TEXT("baseline_invalid"));
                TArray<FPGSkillDataRow*> Rows; Backup->GetAllRows(TEXT("P0 comparison"), Rows);
                TArray<FPGSkillDataRow> Selected;
                for (int32 Id : {100, 101, 102, 111, 112})
                {
                    auto** Found = Rows.FindByPredicate([Id](const FPGSkillDataRow* Row) { return Row->SkillID == Id; });
                    if (!Found || !(*Found)->PlayerProfile.IsNull() || !Tables->GetRowData<FPGSkillDataRow>(Id))
                        return State->Finish(TEXT("baseline_invalid"));
                    Selected.Add(**Found);
                }
                for (const auto& Row : Selected) *Tables->GetRowData<FPGSkillDataRow>(Row.SkillID) = Row;
            }
            auto* ASC = Player->GetPGAbilitySystemComponent();
            ASC->CancelAbilities(); ASC->ClearBufferedInput();
            ASC->SetEquipmentBonuses({}); ASC->SetProfileBonuses({}); ASC->SetCombatPerks({});
            // Isolated QA presets. No lifesteal/auto-heal; these do not change live reward data.
            if (State->Build == TEXT("bleed")) ASC->SetCombatPerks({{EPGCombatPerk::Bleed,10}, {EPGCombatPerk::BleedBurst,100}, {EPGCombatPerk::BleedRecast,1}});
            if (State->Build == TEXT("shock")) ASC->SetCombatPerks({{EPGCombatPerk::Shockwave,30}, {EPGCombatPerk::ShockEcho,50}, {EPGCombatPerk::ShockFracture,1}});
            if (State->Build == TEXT("frenzy")) ASC->SetCombatPerks({{EPGCombatPerk::Frenzy,3}, {EPGCombatPerk::FrenzyAfterimage,1}});
            ASC->SetNumericAttributeBase(UPGAtrributeSet::GetMaxHealthAttribute(), 1000.f);
            ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 1000.f);
            ASC->SetNumericAttributeBase(UPGAtrributeSet::GetAttackPowerAttribute(), 100.f);
            ASC->SetNumericAttributeBase(UPGAtrributeSet::GetDefensePowerAttribute(), 0.f);
            ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCriticalRateAttribute(), 0.f);
            auto* Handler = Player->GetSkillHandler();
            for (auto Slot : {EPGSkillSlot::NormalAttack, EPGSkillSlot::SkillSlot_1, EPGSkillSlot::SkillSlot_2}) Handler->RemoveSkill(Slot);
            Handler->AddSkill(EPGSkillSlot::NormalAttack, 100);
            Handler->AddSkill(EPGSkillSlot::SkillSlot_1, 111); Handler->AddSkill(EPGSkillSlot::SkillSlot_2, 112);
            for (const auto& Pair : Handler->GetAllSkillData()) Handler->GetSkillData(Pair.Key)->LastSkillUsedTime = -1.e30;
            Handler->ResetCombo(); Player->GetPlayerAttackComponent()->PrepareLoadout();
            for (int32 Id : {100, 101, 102, 111, 112})
            {
                const auto* Row = Tables->GetRowData<FPGSkillDataRow>(Id);
                if (!Row || Row->PlayerProfile.IsNull() != (State->Variant == TEXT("baseline")))
                    return State->Finish(TEXT("variant_mismatch"));
                UE_LOG(LogTemp, Display, TEXT("PGSkillTrial ROW skill=%d profile=%d cooldown=%d"), Id, !Row->PlayerProfile.IsNull(), Row->SkillCoolTime);
            }
            Player->SetActorRotation(FRotator::ZeroRotator);
            Player->GetCharacterMovement()->StopMovementImmediately();
            State->Player = Player;
            const bool bElite = State->Scenario == TEXT("P0-E1");
            const bool bRing = State->Scenario == TEXT("P0-RING");
            const int32 Melee = bElite ? 1 : bRing ? 12 : State->Scenario == TEXT("P0-M15") ? 15 : 10;
            const int32 Ranged = bElite ? 0 : Melee == 15 ? 3 : 2;
            FRandomStream Random(State->Seed);
            TWeakPtr<FPGSkillTrial> Weak = State;
            const auto Observe = [State, Weak](UPGAbilitySystemComponent* TargetASC, int32 Index, int32 EnemyID)
            {
                const auto Handle = TargetASC->GetGameplayAttributeValueChangeDelegate(UPGAtrributeSet::GetCurrentHealthAttribute()).AddLambda(
                    [Weak, Index, EnemyID](const FOnAttributeChangeData& Change)
                    {
                        const auto S = Weak.Pin();
                        if (!S || !S->bReady || S->bFinished || !S->World.IsValid()) return;
                        const float Loss = FMath::Max(0.f, Change.OldValue - Change.NewValue);
                        if (Loss <= 0.f) return;
                        if (Index < 0) S->Taken += Loss; else S->Damage += Loss;
                        if (Index >= 0 && Change.NewValue <= 0.f && !S->Dead.Contains(Index)) { S->Dead.Add(Index); ++S->Kills; }
                        const FString ActiveCast = Index < 0 && S->Player.IsValid() ? S->Player->GetPGAbilitySystemComponent()->GetObservedCastId() : TEXT("none");
                        UE_LOG(LogTemp, Display, TEXT("PGSkillTrial HEALTH target=%d enemy=%d time=%.3f loss=%.3f hp=%.3f active_cast=%s"),
                            Index, EnemyID, S->World->GetTimeSeconds() - S->Started, Loss, Change.NewValue, *ActiveCast);
                    });
                State->HealthHandles.Emplace(TargetASC, Handle);
            };
            Observe(ASC, -1, 0);
            const FVector Origin = Player->GetActorLocation();
            for (int32 I = 0; I < Melee + Ranged; ++I)
            {
                const int32 Id = bElite ? 15104 : I < Melee ? 15101 : 15102;
                const auto* Row = Tables->GetRowData<FPGEnemyDataRow>(Id);
                auto* Class = Row ? Row->ActorClass.LoadSynchronous() : nullptr;
                if (!Class) return State->Finish(TEXT("enemy_missing"));
                FVector Offset;
                if (bElite) Offset = FVector(450, 0, 0);
                else if (bRing && I < Melee)
                {
                    const float Angle = 2.f * PI * I / Melee;
                    Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Random.FRandRange(360.f, 400.f);
                }
                else if (I < Melee) Offset = FVector(350 + (I / 5) * 160, (I % 5 - 2) * 145, 0);
                else Offset = FVector(1000, (I - Melee - (Ranged - 1) * .5f) * 260, 0);
                Offset += FVector(Random.FRandRange(-15, 15), Random.FRandRange(-15, 15), 0);
                FNavLocation Point;
                if (!Nav->ProjectPointToNavigation(Origin + Offset, Point, FVector(70, 70, 250)))
                    return State->Finish(TEXT("navigation_failed"));
                FActorSpawnParameters Params;
                Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
                const float Height = Class->GetDefaultObject<APGCharacterEnemy>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                auto* Enemy = World->SpawnActor<APGCharacterEnemy>(Class, Point.Location + FVector(0, 0, Height + 2), FRotator(0, 180, 0), Params);
                if (!Enemy) return State->Finish(TEXT("spawn_failed"));
                State->Enemies.Add(Enemy); Enemy->bCanDropLoot = false;
                if (!Cast<APGRoleAIController>(Enemy->GetController())) return State->Finish(TEXT("ai_missing"));
                Enemy->Tags.Add(TEXT("PGSkillTrial"));
                auto* EnemyASC = Enemy->GetPGAbilitySystemComponent();
                const float HP = bElite ? 5000.f : I < Melee ? 250.f : 200.f;
                EnemyASC->SetNumericAttributeBase(UPGAtrributeSet::GetMaxHealthAttribute(), HP);
                EnemyASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), HP);
                EnemyASC->SetNumericAttributeBase(UPGAtrributeSet::GetDefensePowerAttribute(), 0.f);
                Observe(EnemyASC, I, Id);
                UE_LOG(LogTemp, Display, TEXT("PGSkillTrial SPAWN index=%d enemy=%d hp=%.1f x=%.3f y=%.3f z=%.3f"), I, Id, HP,
                    Enemy->GetActorLocation().X, Enemy->GetActorLocation().Y, Enemy->GetActorLocation().Z);
            }
            if (auto* Messages = UPGMessageManager::Get(Player))
            {
                State->Messages = Messages;
                State->SkillHandle = Messages->RegisterDelegate(EPGPlayerMessageType::UseSkill,
                    TDelegate<void(const IPGEventData*)>::CreateLambda([Weak](const IPGEventData* Event)
                    {
                        const auto S = Weak.Pin();
                        if (!S || !S->bReady || S->bFinished || !S->World.IsValid()) return;
                        const auto* Data = static_cast<const FPGEventDataTwoParam<PGSkillId, EPGSkillSlot>*>(Event);
                        ++S->Uses;
                        UE_LOG(LogTemp, Display, TEXT("PGSkillTrial USE skill=%d time=%.3f"), Data->ValueA, S->World->GetTimeSeconds() - S->Started);
                    }));
                FPGStagePresentation View; View.bClose = true;
                Messages->SendMessage(EPGUIMessageType::StagePresentation, &View);
                Messages->SendMessage(EPGPlayerMessageType::LoadoutChanged, nullptr);
            }
            if (auto* UI = UPGUIManager::Get(Player)) UI->CloseAllUI();
            State->Started = World->GetTimeSeconds(); State->bReady = true;
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
                FString::Printf(TEXT("%s / %s / seed %d - READY (100 + 111 + 112)"), *State->Scenario, *State->Variant, State->Seed));
            UE_LOG(LogTemp, Display, TEXT("PGSkillTrial BEGIN scenario=%s seed=%d variant=%s enemies=%d attack=%.1f hp=%.1f assisted_profile=1 combat_assistance=0 contact_injected=0 smoke=%d build=%s world=%.6f metrics=1"),
                *State->Scenario, State->Seed, *State->Variant, State->Enemies.Num(), ASC->GetCombatStat(EPGStatType::Attack), ASC->GetHealth(), State->bSmoke, *State->Build, State->Started);
        }
        if (!Player || Player != State->Player.Get()) return State->Finish(TEXT("player_lost"));
        if (!State->bCaptured && World->GetTimeSeconds() - State->Started >= .5)
        {
            FString ImagePath;
            if (FParse::Value(FCommandLine::Get(), TEXT("PGSkillTrialScreenshot="), ImagePath))
                FScreenshotRequest::RequestScreenshot(ImagePath, true, false);
            State->bCaptured = true;
        }
        if (Player->GetPGAbilitySystemComponent()->GetHealth() <= 0.f) return State->Finish(TEXT("death"));
        if (State->Kills == State->Enemies.Num()) return State->Finish(TEXT("clear"));
        for (int32 I = 0; I < State->Enemies.Num(); ++I)
            if (!State->Enemies[I].IsValid() && !State->Dead.Contains(I)) return State->Finish(TEXT("target_lost"));
        const double Limit = State->bSmoke ? 3. : State->Scenario == TEXT("P0-E1") ? 30. : 60.;
        if (World->GetTimeSeconds() - State->Started >= Limit) return State->Finish(State->bSmoke ? TEXT("smoke_complete") : TEXT("timeout"));
        return true;
    }));
#endif
}
