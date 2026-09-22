// Fill out your copyright notice in the Description page of Project Settings.


#include "Cheat/PGCheatManager.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGPlayerCombatComponent.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Progression/PGLootDrop.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"

#include "PGCheatComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "PGUI/Widget/Window/PGUIWindowRewardSelect.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"
#include "EngineUtils.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "Component/PGItemCheatComponent.h"
#include "Component/PGObjectCheatComponent.h"
#include "Component/PGStatCheatComponent.h"
#include "Component/PGUICheatComponent.h"

void UPGCheatManager::BeginDestroy()
{
	_components.Empty();
	
	Super::BeginDestroy();
}

void UPGCheatManager::InitCheatManager()
{
	Super::InitCheatManager();

	RegisterComponents(NewObject<UPGItemCheatComponent>(this));
	RegisterComponents(NewObject<UPGObjectCheatComponent>(this));
	RegisterComponents(NewObject<UPGStatCheatComponent>(this));
	RegisterComponents(NewObject<UPGUICheatComponent>(this));
}

bool UPGCheatManager::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	bool Handled = Super::ProcessConsoleExec(Cmd, Ar, Executor);

	if (false == Handled)
	{
		for (TSoftObjectPtr<UPGCheatComponent> Component : _components)
		{
			if (nullptr == Component)
			{
				continue;
			}
			Component->ProcessConsoleExec(Cmd, Ar, Executor);
		}
	}

	return Handled;
}

void UPGCheatManager::RegisterComponents(UPGCheatComponent* NewComponent)
{
	if(nullptr == NewComponent)
	{
		return;
	}

	_components.Emplace(NewComponent);
}

void UPGCheatManager::PGStageStatus()
{
#if !UE_BUILD_SHIPPING
    for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It)
        UE_LOG(LogTemp, Display, TEXT("PG Stage=%d State=%d Wave=%d/%d Remaining=%d Spawned=%d BuildRemaining=%.1f"), It->GetCurrentStageId(), static_cast<int32>(It->GetCurrentStageState()), It->GetCurrentWaveNumber(), It->GetWaveCount(), It->GetRemainingMonsters(), It->GetSpawnedMonsters(), It->GetBuildTimeRemaining());
#endif
}

void UPGCheatManager::PGCombatControlsProbe()
{
#if !UE_BUILD_SHIPPING
    FString Profile;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), Profile)) return;
    for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It) It->StartStage(1);
    for (const float Delay : {2.f, 8.f, 16.f})
    {
        FTimerHandle Timer;
        GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Delay]()
        {
            auto* Player = Cast<APGCharacterPlayer>(GetOuterAPlayerController()->GetPawn());
            if (!Player) return;
            auto* ASC = Player->GetPGAbilitySystemComponent();
            ASC->SetNumericAttributeBase(UPGAtrributeSet::GetMaxHealthAttribute(), 1000000.f);
            ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 1000000.f);
            auto* Weapon = Player->GetPlayerCombatComponent()->GetCharacterCurrentEquippedWeapon();
            UE_LOG(LogTemp, Display, TEXT("PGControls t=%.0f player=%s weapon=%s socket=%s unequip=%d"), Delay,
                *Player->GetActorLocation().ToString(), *GetNameSafe(Weapon),
                Weapon ? *Weapon->GetRootComponent()->GetAttachSocketName().ToString() : TEXT("None"),
                ASC->TryActivateAbilityByTag(PGGamePlayTags::Player_Ability_UnEquip_Weapon));
            auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
            FNavLocation Projected;
            UE_LOG(LogTemp, Display, TEXT("PGControls nav=%d building=%d playerProjection=%d"), !!Nav,
                UNavigationSystemV1::IsNavigationBeingBuilt(this),
                Nav && Nav->ProjectPointToNavigation(Player->GetActorLocation(), Projected, FVector(500.f)));
            for (TActorIterator<ARecastNavMesh> It(GetWorld()); It; ++It)
                UE_LOG(LogTemp, Display, TEXT("PGControls recast=%s generation=%d tiles=%d bounds=%s"), *It->GetName(),
                    static_cast<int32>(It->GetRuntimeGenerationMode()), It->GetNavMeshTilesCount(), *It->GetNavMeshBounds().ToString());
            for (TActorIterator<ANavMeshBoundsVolume> It(GetWorld()); It; ++It)
                UE_LOG(LogTemp, Display, TEXT("PGControls navVolume=%s bounds=%s"), *It->GetName(), *It->GetComponentsBoundingBox(true).ToString());
            for (const auto& Spec : ASC->GetActivatableAbilities())
                UE_LOG(LogTemp, Display, TEXT("PGControls ability=%s tags=%s"), *GetNameSafe(Spec.Ability), *Spec.Ability->GetAssetTags().ToString());
            for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It)
            {
                auto* AI = Cast<AAIController>(It->GetController());
                auto* BB = AI ? AI->GetBlackboardComponent() : nullptr;
                auto* Path = Nav ? UNavigationSystemV1::FindPathToActorSynchronously(this, It->GetActorLocation(), Player, 50.f, AI) : nullptr;
                UE_LOG(LogTemp, Display, TEXT("PGControls enemy=%s pos=%s speed=%.0f max=%.0f mode=%d move=%d target=%s skill=%d dist=%.0f nav=%d path=%d partial=%d"),
                    *It->GetName(), *It->GetActorLocation().ToString(), It->GetVelocity().Size2D(), It->GetCharacterMovement()->MaxWalkSpeed,
                    static_cast<int32>(It->GetCharacterMovement()->MovementMode), AI ? static_cast<int32>(AI->GetMoveStatus()) : -1,
                    BB ? *GetNameSafe(BB->GetValueAsObject(TEXT("TargetActor"))) : TEXT("None"), BB ? BB->GetValueAsInt(TEXT("SelectedSkillID")) : 0,
                    FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()), Nav && Nav->GetDefaultNavDataInstance(),
                    Path && Path->IsValid(), Path && Path->IsPartial());
            }
            if (Delay >= 16.f && FParse::Param(FCommandLine::Get(), TEXT("PGControlsExit"))) FPlatformMisc::RequestExit(false);
        }), Delay, false);
    }
#endif
}
void UPGCheatManager::PGStartStage(int32 StageId)
{
#if !UE_BUILD_SHIPPING
    for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It) { It->StartStage(StageId); break; }
#endif
}
void UPGCheatManager::PGCombatStats()
{
#if !UE_BUILD_SHIPPING
    if (const APGCharacterBase* Player = Cast<APGCharacterBase>(GetOuterAPlayerController()->GetPawn()))
    {
        const auto* ASC = Player->GetPGAbilitySystemComponent();
        UE_LOG(LogTemp, Display, TEXT("PG HP=%.1f/%.1f Attack=%.1f Defense=%.1f Crit=%.1f"), ASC->GetHealth(), ASC->GetCombatStat(EPGStatType::Health), ASC->GetCombatStat(EPGStatType::Attack), ASC->GetCombatStat(EPGStatType::Defense), ASC->GetCombatStat(EPGStatType::CriticalRate));
    }
#endif
}

void UPGCheatManager::PGProfileStatus()
{
#if !UE_BUILD_SHIPPING
    if (auto* Profile = UPGProfileSubsystem::Get(this))
    {
        const auto* Save = Profile->GetProfile();
        UE_LOG(LogTemp, Display, TEXT("PGProfile version=%d revision=%lld items=%d equipped=%d checkpoint=%d clears=%d build=%s status=%s"),
            Save->Version, Save->Revision, Save->Items.Num(), Save->Equipment.Num(), Save->Checkpoint, Save->ClearedStages, *Save->BuildId.ToString(), *Profile->Status);
    }
#endif
}
void UPGCheatManager::PGSaveFailure(bool bFail)
{
#if !UE_BUILD_SHIPPING
    if (auto* Profile = UPGProfileSubsystem::Get(this)) Profile->bInjectSaveFailure = bFail;
#endif
}
void UPGCheatManager::PGDropItem(int32 ItemId, int32 Seed)
{
#if !UE_BUILD_SHIPPING
    auto* Profile = UPGProfileSubsystem::Get(this);
    APawn* Pawn = GetOuterAPlayerController()->GetPawn();
    const auto* Def = Profile && Profile->GetCatalog() ? Profile->GetCatalog()->FindItem(ItemId) : nullptr;
    if (!Def || !Pawn) return;
    FRandomStream Random(Seed); FPGItemInstance Item; Item.Guid = FGuid::NewGuid(); Item.DefinitionId = ItemId; Item.Options = Def->BaseOptions;
    TArray<EPGStatType> Keys; Item.Options.GetKeys(Keys); Keys.Sort();
    for (auto Key : Keys) Item.Options[Key] += Random.RandRange(0, Def->RollBonus);
    auto* Drop = GetWorld()->SpawnActor<APGLootDrop>(Pawn->GetActorLocation() + FVector(100,0,0), FRotator::ZeroRotator);
    if (Drop) Drop->InitializeItem(Item);
#endif
}
void UPGCheatManager::PGFarmingSmoke()
{
#if !UE_BUILD_SHIPPING
    // Explicit isolated profile is mandatory: this automation must not change the player's save.
    FString ProfileName;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), ProfileName)) return;
    FTimerHandle Timer;
    GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        auto* Profile = UPGProfileSubsystem::Get(this);
        APawn* Pawn = GetOuterAPlayerController()->GetPawn();
        if (!Profile || !Pawn) return;
        PGProfileStatus(); PGCombatStats();
        PGDropItem(3403, 1234);
        bool bPickup = false;
        for (TActorIterator<APGLootDrop> It(GetWorld()); It; ++It) if (It->TryPickup(Pawn)) { bPickup = true; break; }
        bool bEquip = false;
        if (!Profile->GetProfile()->Items.IsEmpty()) bEquip = Profile->Equip(Profile->GetProfile()->Items.Last().Guid);
        const auto Before = Profile->GetProfile()->Equipment;
        Profile->bInjectSaveFailure = true;
        const bool bFailureRejected = !Profile->Unequip(EPGEquipmentSlot::Weapon);
        Profile->bInjectSaveFailure = false;
        bool bReward = false;
        for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It)
            It->GetPGAbilitySystemComponent()->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 0.f);
        for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It)
            if (It->CurrentStageState == EPGStageState::BuildPhase) bReward = It->CommitReward(It->RewardToken, It->OfferedRewards.IsEmpty() ? INDEX_NONE : 0);
        float BaseCooldown = 0.f, GrownCooldown = 0.f;
        bool bGrowth = false;
        if (auto* Character = Cast<APGCharacterBase>(Pawn))
        {
            if (Profile->SelectBuild(TEXT("Rapid1")))
                if (auto* Skill = Character->GetSkillHandler()->GetSkillData(EPGSkillSlot::SkillSlot_1)) BaseCooldown = Skill->CoolTime;
            if (Profile->SelectBuild(TEXT("Rapid2")))
                if (auto* Skill = Character->GetSkillHandler()->GetSkillData(EPGSkillSlot::SkillSlot_1)) GrownCooldown = Skill->CoolTime;
            bGrowth = GrownCooldown > 0.f && GrownCooldown < BaseCooldown;
        }
        UE_LOG(LogTemp, Display, TEXT("PGFarmingSmoke pickup=%d equip=%d failedSaveRejected=%d reward=%d growth=%d cooldown=%.2f->%.2f"), bPickup, bEquip, bFailureRejected, bReward, bGrowth, BaseCooldown, GrownCooldown);
        PGProfileStatus(); PGCombatStats();
        FTimerHandle ScreenshotTimer;
        GetWorld()->GetTimerManager().SetTimer(ScreenshotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            if (auto* PC = Cast<APGPlayerController>(GetOuterAPlayerController())) PC->ToggleInventory();
            GetOuterAPlayerController()->ConsoleCommand(TEXT("Shot SHOWUI"));
        }), 2.f, false);
    }), 3.f, false);
#endif
}
void UPGCheatManager::PGStress(int32 EnemyCount, int32 DropCount)
{
#if !UE_BUILD_SHIPPING
    EnemyCount = FMath::Clamp(EnemyCount, 0, 200); DropCount = FMath::Clamp(DropCount, 0, 200);
    APGStageManager* Stage = nullptr;
    for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It) { Stage = *It; break; }
    auto* Data = UPGDataTableManager::Get(this);
    if (!Stage || !Data) return;
    const auto Enemies = Data->GetAllRowData<FPGEnemyDataRow>();
    if (Enemies.IsEmpty()) return;
    int32 Spawned = 0;
    for (int32 Index = 0; Index < EnemyCount; ++Index) if (Stage->SpawnSingleEnemy(Enemies[Index % Enemies.Num()]->EnemyID)) ++Spawned;
    for (int32 Index = 0; Index < DropCount; ++Index) PGDropItem(3401, Index);
    if (auto* Character = Cast<APGCharacterBase>(GetOuterAPlayerController()->GetPawn()))
    {
        Character->GetPGAbilitySystemComponent()->SetNumericAttributeBase(UPGAtrributeSet::GetMaxHealthAttribute(), 1000000.f);
        Character->GetPGAbilitySystemComponent()->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 1000000.f);
    }
    UE_LOG(LogTemp, Display, TEXT("PGStress enemies=%d drops=%d seed=0..%d"), Spawned, DropCount, DropCount-1);
#endif
}

void UPGCheatManager::PGFeedbackStatus()
{
#if !UE_BUILD_SHIPPING
    if (auto* Character = Cast<APGCharacterBase>(GetOuterAPlayerController()->GetPawn()))
        UE_LOG(LogTemp, Display, TEXT("PGFeedback hitStopTotal=%.3f suppressed=%d"), Character->GetTotalHitStopSeconds(), Character->GetSuppressedFeedbackRequests());
#endif
}
void UPGCheatManager::PGCombatCycleSmoke()
{
#if !UE_BUILD_SHIPPING
    FString Profile;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), Profile)) return;
    CycleProbeTicks = CycleProbeRewards = CycleProbeWait = 0;
    GetWorld()->GetTimerManager().SetTimer(CombatCycleTimer, this, &ThisClass::TickCombatCycleProbe, 1.f, true, 3.f);
#endif
}
static float PGMeasureCycleDamage(UWorld* World, UPGAbilitySystemComponent* Source)
{
    auto* TargetActor = World->SpawnActor<AActor>();
    auto* Target = NewObject<UPGAbilitySystemComponent>(TargetActor);
    TargetActor->AddInstanceComponent(Target); Target->RegisterComponent(); Target->InitAbilityActorInfo(TargetActor,TargetActor);
    Target->InitializeCombatStats({{EPGStatType::Health,1000000},{EPGStatType::Defense,100}});
    const float Critical = Source->GetNumericAttributeBase(UPGAtrributeSet::GetCriticalRateAttribute());
    Source->SetNumericAttributeBase(UPGAtrributeSet::GetCriticalRateAttribute(),0);
    EPGDamageType Type; const float Damage = Target->ReceiveCombatHit(Source,Type);
    Source->SetNumericAttributeBase(UPGAtrributeSet::GetCriticalRateAttribute(),Critical);
    TargetActor->Destroy(); return Damage;
}
void UPGCheatManager::TickCombatCycleProbe()
{
#if !UE_BUILD_SHIPPING
    APGStageManager* Stage = nullptr;
    for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It) { Stage = *It; break; }
    auto* Character = Cast<APGCharacterBase>(GetOuterAPlayerController()->GetPawn());
    if (!Stage || !Character || ++CycleProbeTicks > 600)
    {
        UE_LOG(LogTemp, Error, TEXT("PGCombatCycle TIMEOUT rewards=%d"), CycleProbeRewards);
        GetWorld()->GetTimerManager().ClearTimer(CombatCycleTimer); return;
    }
    auto* ASC = Character->GetPGAbilitySystemComponent();
    ASC->SetNumericAttributeBase(UPGAtrributeSet::GetMaxHealthAttribute(), 1000000);
    ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 1000000);
    if (Stage->CurrentStageState == EPGStageState::InProgress)
    {
        Stage->WaveStartTime = GetWorld()->GetTimeSeconds() - 100;
        if (++CycleProbeWait < 6) return;
        const auto Enemies = Stage->SpawnedEnemies;
        for (auto* Enemy : Enemies)
            if (IsValid(Enemy)) { Enemy->GetPGAbilitySystemComponent()->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 0); static_cast<APGCharacterBase*>(Enemy)->OnHealthChanged(); }
        CycleProbeWait = 0;
    }
    else if (Stage->CurrentStageState == EPGStageState::BuildPhase && !Stage->bRewardCommitted)
    {
        if (++CycleProbeWait == 1) { GetOuterAPlayerController()->ConsoleCommand(TEXT("Shot SHOWUI")); PGDropItem(3401, 14); return; }
        if (CycleProbeWait < 3) return;
        TArray<UUserWidget*> Windows;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Windows, UPGUIWindowRewardSelect::StaticClass(), false);
        int32 Choice = 0;
        for (int32 I=0; I<Stage->OfferedRewards.Num(); ++I)
            if (const auto* Reward = PGData()->GetRowData<FPGRewardStatDataRow>(Stage->OfferedRewards[I].RewardId); Reward && (Reward->Perk == static_cast<EPGCombatPerk>(CycleProbeRewards + 1) || (Reward->Perk == EPGCombatPerk::None && Reward->StatType == EPGStatType::Attack))) Choice=I;
        UE_LOG(LogTemp, Display, TEXT("PGCombatCycle select stage=%d attack=%.1f token=%s"), Stage->CurrentStageId, ASC->GetCombatStat(EPGStatType::Attack), *Stage->RewardToken.ToString());
        CycleProbeBeforeDamage = PGMeasureCycleDamage(GetWorld(), ASC);
        UPGUIWindowRewardSelect* ActiveWindow = nullptr;
        for (auto* Widget : Windows)
            if (auto* Window = Cast<UPGUIWindowRewardSelect>(Widget); Window && Window->IsInViewport() && Window->Token == Stage->RewardToken) { ActiveWindow = Window; break; }
        if (ActiveWindow && !FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) ActiveWindow->BeginChoice(Choice);
        else if (!Stage->CommitReward(Stage->RewardToken, Stage->OfferedRewards.IsEmpty() ? INDEX_NONE : Choice)) UE_LOG(LogTemp, Error, TEXT("PGCombatCycle reward rejected"));
        ++CycleProbeRewards; CycleProbeWait=0;
    }
    else if (Stage->CurrentStageState == EPGStageState::BuildPhase && Stage->bRewardCommitted && CycleProbeBeforeDamage > 0)
    {
        const float After = PGMeasureCycleDamage(GetWorld(), ASC);
        UE_LOG(LogTemp, Display, TEXT("PGCombatCycle damage %.2f -> %.2f growth=%d"),CycleProbeBeforeDamage,After,After > CycleProbeBeforeDamage);
        UE_LOG(LogTemp, Display, TEXT("PGCombatCycle perks leech=%d execution=%d counter=%d"), ASC->GetPerkPercent(EPGCombatPerk::LifeSteal), ASC->GetPerkPercent(EPGCombatPerk::Execution), ASC->GetPerkPercent(EPGCombatPerk::Counter));
          CycleProbeBeforeDamage=0;
          if (Stage->IsManualReady()) Stage->ReadyForNextStage();
    }
    else if (Stage->CurrentStageState == EPGStageState::Finished)
    {
        UE_LOG(LogTemp, Display, TEXT("PGCombatCycle COMPLETE rewards=%d attack=%.1f"), CycleProbeRewards, ASC->GetCombatStat(EPGStatType::Attack));
        PGFeedbackStatus(); PGProfileStatus(); GetWorld()->GetTimerManager().ClearTimer(CombatCycleTimer);
    }
    else if (Stage->CurrentStageState == EPGStageState::Failed)
    {
        UE_LOG(LogTemp, Error, TEXT("PGCombatCycle FAILED")); GetWorld()->GetTimerManager().ClearTimer(CombatCycleTimer);
    }
#endif
}
