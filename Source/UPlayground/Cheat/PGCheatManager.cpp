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
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGAI/PGRoleAIController.h"

#include "PGCheatComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "PGUI/Widget/Window/PGUIWindowRewardSelect.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
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
    auto* ProfileSystem = UPGProfileSubsystem::Get(this);
    if (!ProfileSystem || !ProfileSystem->MarkRunAssisted()) return;
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
            if (Delay == 8.f && FParse::Param(FCommandLine::Get(), TEXT("PGCaptureProbe"))) GetOuterAPlayerController()->ConsoleCommand(TEXT("Shot SHOWUI"));
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

void UPGCheatManager::PGBossDamage(float Amount)
{
#if !UE_BUILD_SHIPPING
    auto* Profile = UPGProfileSubsystem::Get(this);
    auto* Player = Cast<APGCharacterBase>(GetOuterAPlayerController()->GetPawn());
    auto* Tables = UPGDataTableManager::Get(this);
    if (!Player || !Tables || !FMath::IsFinite(Amount) || Amount <= 0 || !Profile || !Profile->MarkRunAssisted()) return;
    for (TActorIterator<APGCharacterEnemy> It(GetWorld()); It; ++It)
        if (const auto* Row = Tables->GetRowData<FPGEnemyDataRow>(It->GetCharacterTID()); Row && Row->Role == EPGEnemyRole::Boss)
        {
            const float Damage = It->GetPGAbilitySystemComponent()->ReceiveProcDamage(Player->GetPGAbilitySystemComponent(), Amount);
            UE_LOG(LogTemp, Display, TEXT("PGBoss Probe damage=%.1f health=%.1f phase=%d transition=%d"),
                Damage, It->GetPGAbilitySystemComponent()->GetHealth(), It->BossPhase, It->IsBossTransitioning());
        }
#endif
}

void UPGCheatManager::PGBuildScenario(FString Family, bool bCore)
{
#if !UE_BUILD_SHIPPING
    FString Slot;
    if (!FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),Slot) || Slot.IsEmpty()) return;
    const int32 Index = Family.Equals(TEXT("Bleed"),ESearchCase::IgnoreCase) ? 0 : Family.Equals(TEXT("Shock"),ESearchCase::IgnoreCase) ? 1 : Family.Equals(TEXT("Frenzy"),ESearchCase::IgnoreCase) ? 2 : -1;
    auto* Profile=UPGProfileSubsystem::Get(this);
    auto* Player=Cast<APGCharacterPlayer>(GetOuterAPlayerController()->GetPawn());
    auto* Tables=UPGDataTableManager::Get(this);
    if (Index<0 || !Profile || !Player || !Tables) return;
    const int32 Root=15000+Index*4;
    TArray<int32> Rewards={Root,Root+(Index==0 ? 3 : 2)};
    if (bCore) Rewards.Add(15018+Index);
    if (!Profile->ConfigureBuildScenario(Rewards)) return;
    for(TActorIterator<APGStageManager> It(GetWorld());It;++It)
    {
        It->CurrentStageState=EPGStageState::None;
        It->GetWorldTimerManager().ClearAllTimersForObject(*It);
        It->CloseRewardWindow();
        It->ClearAllEnemies();
        It->MonsterSpawnQueue.Reset(); It->ActiveWaves.Reset(); It->CurrentWaveIndex=INDEX_NONE;
        It->RemainingMonsters=0; It->RewardToken.Invalidate(); It->bRewardCommitted=false;
        It->PrepareRun(1);
        It->CurrentStageState=EPGStageState::InProgress;
        break;
    }
    TArray<AActor*> Old;
    for(TActorIterator<APGCharacterEnemy> It(GetWorld());It;++It) if(It->ActorHasTag(TEXT("PGBuildScenario"))) Old.Add(*It);
    for(auto* Actor:Old) Actor->Destroy();
    auto* ASC=Player->GetPGAbilitySystemComponent();
    ASC->CancelAbilities();
    ASC->SetCombatPerks({}); Profile->RestorePlayer(Player);
    for (const auto& Pair : Player->GetSkillHandler()->GetAllSkillData())
        Player->GetSkillHandler()->GetSkillData(Pair.Key)->LastSkillUsedTime=0.;
    ASC->SetNumericAttributeBase(UPGAtrributeSet::GetAttackPowerAttribute(),100.f);
    ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCriticalRateAttribute(),0.f);
    // Weapon modifiers remain separate from the profile. Normalize evaluated stats for comparison.
    ASC->SetNumericAttributeBase(UPGAtrributeSet::GetAttackPowerAttribute(),200.f-ASC->GetCombatStat(EPGStatType::Attack));
    ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCriticalRateAttribute(),-ASC->GetCombatStat(EPGStatType::CriticalRate));
    ASC->RestoreHealth(ASC->GetCombatStat(EPGStatType::Health));
    if(BuildScenarioWorld != GetWorld()) { BuildScenarioWorld=GetWorld(); BuildScenarioOrigin=Player->GetActorLocation(); }
    Player->SetActorLocation(BuildScenarioOrigin,false,nullptr,ETeleportType::TeleportPhysics);
    Player->SetActorRotation(FRotator::ZeroRotator);
    const auto* Row=Tables->GetRowData<FPGEnemyDataRow>(15101);
    UClass* Class=Row ? Row->ActorClass.LoadSynchronous() : nullptr;
    if (!Class) return;
    for(int32 I=0;I<3;++I)
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Enemy=GetWorld()->SpawnActor<APGCharacterEnemy>(Class,Player->GetActorLocation()+FVector(170,(I-1)*100,0),FRotator(0,180,0),Params);
        if(!Enemy) continue;
        Enemy->Tags.Add(TEXT("PGBuildScenario"));
        if(auto* AI=Cast<APGRoleAIController>(Enemy->GetController())) AI->SetCombatThinkingEnabled(false);
        Enemy->GetPGAbilitySystemComponent()->SetNumericAttributeBase(UPGAtrributeSet::GetMaxHealthAttribute(),10000.f);
        Enemy->GetPGAbilitySystemComponent()->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),10000.f);
        Enemy->GetPGAbilitySystemComponent()->SetNumericAttributeBase(UPGAtrributeSet::GetDefensePowerAttribute(),100.f);
    }
    UE_LOG(LogTemp,Display,TEXT("PGBuildScenario family=%s core=%d targets=3 assisted=1"),*Family,bCore);
    PGCombatStats();
#endif
}

void UPGCheatManager::PGBuildProbe(FString Action)
{
#if !UE_BUILD_SHIPPING
    FString Slot;
    if (!FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),Slot)) return;
    auto* Profile=UPGProfileSubsystem::Get(this);
    auto* Player=Cast<APGCharacterPlayer>(GetOuterAPlayerController()->GetPawn());
    if (!Profile || !Player || !Profile->MarkRunAssisted()) return;
    auto* Source=Player->GetPGAbilitySystemComponent();
    TArray<UPGAbilitySystemComponent*> Targets;
    for(TActorIterator<APGCharacterEnemy> It(GetWorld());It;++It)
        if(It->ActorHasTag(TEXT("PGBuildScenario")) && It->GetPGAbilitySystemComponent()->GetHealth()>0) Targets.Add(It->GetPGAbilitySystemComponent());
    EPGDamageType Type;
    if(Action==TEXT("hit")) { Source->BeginCombatSkill(EPGSkillSlot::NormalAttack); for(auto* Target:Targets) Target->ReceiveCombatHit(Source,Type); }
    if(Action==TEXT("heavy"))
    {
        if(auto* Skill=Player->GetSkillHandler()->GetSkillData(EPGSkillSlot::SkillSlot_1)) { Skill->CoolTime=10; Skill->LastSkillUsedTime=FPlatformTime::Seconds(); }
        Source->BeginCombatSkill(EPGSkillSlot::SkillSlot_1);
        for(auto* Target:Targets) Target->ReceiveCombatHit(Source,Type);
        Source->SetHeavySkill(false);
    }
    if(Action==TEXT("burst_setup")) for(auto* Target:Targets) Target->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(),65.f);
    if(Action==TEXT("dodge")) Source->OnDodgeCommitted();
    if(Action==TEXT("roll"))
    {
        const auto* Viewport=GetWorld()->GetGameViewport();
        const auto* Roll=Player->GetSkillHandler()->GetSkillData(EPGSkillSlot::SkillSlot_Roll);
        UE_LOG(LogTemp,Display,TEXT("PGBuildRoll allowed=%d control=%d moveIgnored=%d focus=%d cooldown=%.2f"),
            Player->IsGameplayInputAllowed(),Player->GetIsCacControl(),GetOuterAPlayerController()->IsMoveInputIgnored(),
            Viewport && Viewport->Viewport && Viewport->Viewport->HasFocus(),Roll ? Roll->GetRemainingCooldown() : -1);
        for (const auto& Spec : Source->GetActivatableAbilities())
            if(Spec.GetDynamicSpecSourceTags().HasTagExact(PGGamePlayTags::InputTag_Roll))
                UE_LOG(LogTemp,Display,TEXT("PGBuildRoll ability=%s active=%d"),*GetNameSafe(Spec.Ability),Spec.IsActive());
        Source->OnAbilityInputPressed(PGGamePlayTags::InputTag_Roll);
        Source->OnAbilityInputReleased(PGGamePlayTags::InputTag_Roll);
    }
    const auto State=Source->GetBuildCombatState();
    UE_LOG(LogTemp,Display,TEXT("PGBuildProbe action=%s targets=%d bleed=%d frenzy=%d shock=%d weak=%.2f refund=%d afterimage=%d cooldown=%.2f"),
        *Action,Targets.Num(),State.BleedStacks,State.FrenzyStacks,State.ShockHits,State.WeaknessSeconds,State.bRefundProc,State.bAfterimageProc,
        Player->GetSkillHandler()->GetSkillData(EPGSkillSlot::SkillSlot_1) ? Player->GetSkillHandler()->GetSkillData(EPGSkillSlot::SkillSlot_1)->GetRemainingCooldown() : 0);
#endif
}

void UPGCheatManager::PGBuildCards()
{
#if !UE_BUILD_SHIPPING
    FString Slot;
    if (!FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),Slot)) return;
    for(TActorIterator<APGStageManager> It(GetWorld());It;++It)
    {
        if(auto* Tables=UPGDataTableManager::Get(this)) if(const auto* Row=Tables->GetRowData<FPGStageDataRow>(4))
        { It->CurrentStageDataCache=*Row; It->CurrentStageId=4; It->CurrentStageState=EPGStageState::BuildPhase; It->RewardsRemaining=2; It->ShowRewardSelection(); }
        break;
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
    auto* ProfileSystem = UPGProfileSubsystem::Get(this);
    if (!ProfileSystem || !ProfileSystem->MarkRunAssisted()) return;
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
    auto* ProfileSystem = UPGProfileSubsystem::Get(this);
    if (!ProfileSystem || !ProfileSystem->MarkRunAssisted()) return;
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
    auto* ProfileSystem = UPGProfileSubsystem::Get(this);
    if (!ProfileSystem || !ProfileSystem->MarkRunAssisted()) return;
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
