#include "PGStageManager.h"
#include "PGData/DataTable/Reward/PGRewardSelection.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGActor/Progression/PGRunTelemetrySubsystem.h"
#include "PGShared/Shared/Structure/PGRunRandom.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "Engine/Engine.h"
#include "PGData/Validation/PGStageValidation.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGActor/Components/Stat/PGStatComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/NavigationSystemBase.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Enum/PGUIWIdgetEnumTypes.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataTemplate.h"
#include "PGStagePresentation.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarPGSpawnDebug(TEXT("pg.Stage.SpawnDebug"), 0,
    TEXT("Draw and log rejected spawn candidates with their failure reason."));


APGStageManager::APGStageManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void APGStageManager::BeginPlay()
{
    Super::BeginPlay();
    if (UPGMessageManager* Manager = UPGMessageManager::Get(this))
    {
        OnActorDiedHandle = Manager->RegisterDelegate(EPGSharedMessageType::OnDied, this, &ThisClass::OnActorDied);
        OnActorSpawnedHandle = Manager->RegisterDelegate(EPGSharedMessageType::OnSpawned, this, &ThisClass::OnActorSpawned);
        OnPlayerDiedHandle = Manager->RegisterDelegate(EPGPlayerMessageType::Died, this, &ThisClass::OnPlayerDied);
    }
}

void APGStageManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CurrentStageState = EPGStageState::None;
    BossDefeatPresentationUntil = 0;
    GetWorldTimerManager().ClearAllTimersForObject(this);
    CloseRewardWindow();
    if (UPGMessageManager* Manager = UPGMessageManager::Get(this))
    {
        Manager->UnregisterDelegate(EPGSharedMessageType::OnDied, OnActorDiedHandle);
        Manager->UnregisterDelegate(EPGSharedMessageType::OnSpawned, OnActorSpawnedHandle);
        Manager->UnregisterDelegate(EPGPlayerMessageType::Died, OnPlayerDiedHandle);
    }
    Super::EndPlay(EndPlayReason);
}

void APGStageManager::StartStage(int32 StageId)
{
    if (StageId == -1) StageId = CurrentStageId + 1;
    CurrentStageState = EPGStageState::None;
    GetWorldTimerManager().ClearAllTimersForObject(this);
    CloseRewardWindow();
    RewardToken.Invalidate();
    bRewardCommitted = false;
    ClearAllEnemies();
    ActiveWaves.Reset();
    CurrentWaveIndex = INDEX_NONE;
    RemainingMonsters = 0;
    MonsterSpawnQueue.Reset();
    if (auto* PC = Cast<APGPlayerController>(UGameplayStatics::GetPlayerController(this, 0))) PC->CloseInventory();
    PreparedEnemyClasses.Reset();
    if (!LoadStageData(StageId) || !IsValidStageData(CurrentStageDataCache))
    {
        FailStage(TEXT("Stage data is missing or invalid."));
        return;
    }
    for (const FPGStageReward& Reward : CurrentStageDataCache.RewardPool)
    {
        const FPGRewardStatDataRow* Stat = PGData()->GetRowData<FPGRewardStatDataRow>(Reward.RewardId);
        if (Reward.RewardType != EPGRewardType::Stat || !Stat || (Stat->Amount < 0 || (Stat->Amount == 0 && Stat->Perk == EPGCombatPerk::None) || Stat->Perk >= EPGCombatPerk::Max || Stat->PerkPercent < 0 || Stat->PerkPercent > 100) || Stat->StatType <= EPGStatType::None || Stat->StatType >= EPGStatType::Max)
        {
            FailStage(FString::Printf(TEXT("Unsupported or invalid reward %d."), Reward.RewardId));
            return;
        }
    }
    ActiveWaves = CurrentStageDataCache.Waves;
    if (ActiveWaves.IsEmpty())
    {
        FPGStageWave LegacyWave;
        LegacyWave.StartDelay = 0.f;
        LegacyWave.MonsterSpawnInfos = CurrentStageDataCache.MonsterSpawnInfos;
        ActiveWaves.Add(MoveTemp(LegacyWave));
    }
    // Pin every wave's classes before combat begins.
    for (const FPGStageWave& Wave : ActiveWaves)
    for (const FPGMonsterSpawnInfo& Spawn : Wave.MonsterSpawnInfos)
    {
        const FPGEnemyDataRow* Enemy = PGData()->GetRowData<FPGEnemyDataRow>(Spawn.MonsterId);
        UClass* Class = Enemy ? Enemy->ActorClass.LoadSynchronous() : nullptr;
        if (!Class || !Class->IsChildOf(APGCharacterEnemy::StaticClass()))
        {
            FailStage(FString::Printf(TEXT("Missing enemy class: %d"), Spawn.MonsterId));
            return;
        }
        PreparedEnemyClasses.AddUnique(Class);
    }
      CurrentStageId = StageId;
    if (auto* Profile = UPGProfileSubsystem::Get(this))
    {
        if (!Profile->EnsureRunSeed()) { FailStage(TEXT("런 시드 저장 실패")); return; }
        RunSeed = Profile->GetProfile()->RunSeed;
        if (auto* Telemetry = UPGRunTelemetrySubsystem::Get(this))
            Telemetry->StartStage(RunSeed, StageId, Profile->GetProfile()->bAssistedRun);
    }
    SpawnFailureCount = 0;
    SpawnedMonsters = 0;
    PrepareWave(0);
    OnStageStarted.Broadcast(StageId);
    if (UPGMessageManager* Manager = UPGMessageManager::Get(this))
    {
        FPGEventDataOneParam<int32> Data(StageId);
        Manager->SendMessage(EPGUIMessageType::StageChanged, &Data);
    }
}

void APGStageManager::PrepareWave(int32 WaveIndex)
{
    GetWorldTimerManager().ClearTimer(SpawnTimer);
    CurrentWaveIndex = WaveIndex;
    SpawnRandom.Initialize(PGRunRandom::Seed(RunSeed, CurrentStageId, WaveIndex, 1));
    CurrentStageState = EPGStageState::WaveIntermission;
    RemainingMonsters = 0;
    for (const auto& Spawn : ActiveWaves[WaveIndex].MonsterSpawnInfos) RemainingMonsters += Spawn.SpawnCount;
    InitializeMonsterSpawnQueue();
    OnMonsterCountChanged.Broadcast(RemainingMonsters);
    const float Delay = ActiveWaves[WaveIndex].StartDelay;
    if (Delay <= 0.f) StartWave();
    else GetWorldTimerManager().SetTimer(WaveTimer, this, &ThisClass::StartWave, Delay, false);
}

void APGStageManager::StartWave()
{
    if (CurrentStageState != EPGStageState::WaveIntermission) return;
    PrepareNavigationForWave();
    CurrentStageState = EPGStageState::InProgress;
    WaveStartTime = GetWorld()->GetTimeSeconds();
    if (auto* Telemetry = UPGRunTelemetrySubsystem::Get(this)) Telemetry->StartWave();
    SpawnFailureCount = 0;
    UE_LOG(LogTemp, Log, TEXT("PGWave started stage=%d wave=%d/%d remaining=%d"), CurrentStageId, GetCurrentWaveNumber(), GetWaveCount(), RemainingMonsters);
    GetWorldTimerManager().SetTimer(SpawnTimer, this, &ThisClass::SpawnEnemyBatch,
        FMath::Max(0.01f, CurrentStageDataCache.SpawnInterval), true, 0.f);
}

void APGStageManager::PrepareNavigationForWave()
{
    // StartStage clears this actor's timers. A BeginPlay next-tick callback was
    // therefore cancelled by auto-start, leaving duplicated maps with no tiles.
    // Resolve the empty dynamic mesh before the spawn timer, in PIE and -game.
    // Existing baked meshes are never rebuilt; retries and collision rules stay intact.
    if (auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
        if (const auto* Recast = Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance()))
            if (Recast->GetRuntimeGenerationMode() == ERuntimeGenerationType::Dynamic &&
                !Recast->GetNavMeshBounds().IsValid)
            {
                const double Started = FPlatformTime::Seconds();
                Nav->Build();
                UE_LOG(LogTemp, Log, TEXT("PGSpawn navigation world=%s type=%d registeredBounds=%d ready=%d building=%d elapsedMs=%.2f"),
                    *GetWorld()->GetName(), int32(GetWorld()->WorldType), Nav->GetNavigationBounds().Num(),
                    Recast->GetNavMeshBounds().IsValid, Nav->IsNavigationBuildInProgress(), (FPlatformTime::Seconds() - Started) * 1000.);
            }
}

bool APGStageManager::LoadStageData(int32 StageId)
{
	UPGDataTableManager* Manager = PGData();
	if (nullptr == Manager)
	{
		return false;
	}

	FPGStageDataRow* StageData = Manager->GetRowData<FPGStageDataRow>(StageId);
	if (nullptr == StageData)
	{
		return false;
	}
	
	if (!StageData)
	{
		return false;
	}
	
	// 데이터를 값으로 복사
	CurrentStageDataCache = *StageData;
	
	return true;
}

void APGStageManager::InitializeMonsterSpawnQueue()
{
	MonsterSpawnQueue.Empty();
    
	// 각 몬스터 타입별로 스폰 대기열 항목 생성
	for (const FPGMonsterSpawnInfo& SpawnInfo : ActiveWaves[CurrentWaveIndex].MonsterSpawnInfos)
	{
		if (SpawnInfo.SpawnCount > 0)
		{
			FPGMonsterSpawnQueueItem QueueItem(
				SpawnInfo.MonsterId,
				SpawnInfo.SpawnCount,
				SpawnInfo.SpawnDelayTime,
				SpawnInfo.SpawnPriority
			);
            
			MonsterSpawnQueue.Add(QueueItem);
		}
	}
    
	// 우선순위와 딜레이 시간에 따라 정렬
	MonsterSpawnQueue.Sort();
}

void APGStageManager::SpawnEnemyBatch()
{
    if (CurrentStageState != EPGStageState::InProgress) return;
    for (int32 Count = 0; Count < CurrentStageDataCache.SpawnBatchSize; ++Count)
    {
        const int32 QueueIndex = SelectNextMonsterToSpawn();
        if (!MonsterSpawnQueue.IsValidIndex(QueueIndex)) break;
        const int32 MonsterId = MonsterSpawnQueue[QueueIndex].MonsterId;
        APGCharacterEnemy* Enemy = SpawnSingleEnemy(MonsterId);
        if (!Enemy)
        {
            UE_LOG(LogTemp, Warning, TEXT("PGSpawn retry stage=%d wave=%d enemy=%d retry=%d/%d reason=%s location=%s"),
                CurrentStageId, GetCurrentWaveNumber(), MonsterId, SpawnFailureCount + 1, CurrentStageDataCache.MaxSpawnRetries,
                PGSpawnFailureName(LastSpawnFailure), *LastSpawnLocation.ToString());
            if (++SpawnFailureCount >= CurrentStageDataCache.MaxSpawnRetries)
                FailStage(FString::Printf(TEXT("Spawn retries exhausted: %d (%s)"), MonsterId, PGSpawnFailureName(LastSpawnFailure)));
            break;
        }
        SpawnFailureCount = 0;
        ++SpawnedMonsters;
        SpawnedEnemies.AddUnique(Enemy);
        Enemy->OnDestroyed.AddUniqueDynamic(this, &ThisClass::OnTrackedEnemyDestroyed);
        if (--MonsterSpawnQueue[QueueIndex].RemainingCount <= 0) MonsterSpawnQueue.RemoveAt(QueueIndex);
        OnEnemySpawned.Broadcast(Enemy);
        OnMonsterTypeSpawned.Broadcast(MonsterId, GetRemainingSpawnCountForMonsterType(MonsterId));
    }
    if (MonsterSpawnQueue.IsEmpty()) GetWorldTimerManager().ClearTimer(SpawnTimer);
    CheckStageComplete();
}

APGCharacterEnemy* APGStageManager::SpawnSingleEnemy(int32 EnemyId)
{
    LastSpawnFailure = EPGSpawnFailure::Asset;
    LastSpawnLocation = FVector::ZeroVector;
	UPGDataTableManager* Manager = PGData();
	if (nullptr == Manager)
	{
		return nullptr;
	}

	FPGEnemyDataRow* EnemyData = Manager->GetRowData<FPGEnemyDataRow>(EnemyId);
	if (nullptr == EnemyData)
	{
		return nullptr;
	}
	
	if (false == EnemyData->ActorClass.ToSoftObjectPath().IsValid())
	{
		return nullptr;
	}

	// 안전한 스폰 위치 찾기 (최대 5회 시도)
	UClass* EnemyClass = EnemyData->ActorClass.LoadSynchronous();
    if (!EnemyClass || !EnemyClass->IsChildOf(APGCharacterEnemy::StaticClass())) return nullptr;
    const auto* Defaults = EnemyClass->GetDefaultObject<APGCharacterEnemy>();
    const float CapsuleRadius = Defaults->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float CapsuleHalfHeight = Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector SpawnLocation = FVector::ZeroVector;
	bool bFoundValidLocation = false;
	
	for (int32 Attempt = 0; Attempt < 5; ++Attempt)
	{
		SpawnLocation = GetSafeSpawnLocation() + FVector(0.f, 0.f, CapsuleHalfHeight + 2.f);
        LastSpawnLocation = SpawnLocation;
		if (IsValidSpawnLocation(SpawnLocation, CapsuleRadius, CapsuleHalfHeight, LastSpawnFailure))
		{
			bFoundValidLocation = true;
			break;
		}
        LogSpawnFailure(EnemyId, Attempt + 1, LastSpawnFailure, SpawnLocation);
	}
	
	// Keep the queued enemy for retry instead of spawning outside the navigable arena.
	if (!bFoundValidLocation)
	{
		return nullptr;
	}
	
	FRotator SpawnRotation = FRotator::ZeroRotator;
	
	// 적 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	
	APGCharacterEnemy* SpawnedEnemy = GetWorld()->SpawnActor<APGCharacterEnemy>(
		EnemyClass, SpawnLocation, SpawnRotation, SpawnParams);
	
    LastSpawnFailure = SpawnedEnemy ? EPGSpawnFailure::None : EPGSpawnFailure::ActorSpawn;
    if (!SpawnedEnemy) LogSpawnFailure(EnemyId, 0, LastSpawnFailure, SpawnLocation);
	
	return SpawnedEnemy;
}

int32 APGStageManager::SelectNextMonsterToSpawn()
{
    const float Elapsed = GetWorld()->GetTimeSeconds() - WaveStartTime;
    for (int32 Index = 0; Index < MonsterSpawnQueue.Num(); ++Index)
        if (Elapsed >= MonsterSpawnQueue[Index].DelayTime) return Index;
    return INDEX_NONE;
}

FVector APGStageManager::GetRandomSpawnLocation() const
{
	// 플레이어 위치 기준으로 랜덤 스폰 (원형 영역)
	FVector PlayerLocation = GetActorLocation();
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		PlayerLocation = PlayerPawn->GetActorLocation();
	}
	
	// 랜덤 원형 위치 생성
	float RandomAngle = SpawnRandom.FRandRange(0.0f, 360.0f);
	float RandomRadius = SpawnRandom.FRandRange(CurrentStageDataCache.SpawnRadius * 0.7f, CurrentStageDataCache.SpawnRadius);
	
	FVector RandomOffset = FVector(
		FMath::Cos(FMath::DegreesToRadians(RandomAngle)) * RandomRadius,
		FMath::Sin(FMath::DegreesToRadians(RandomAngle)) * RandomRadius,
		0.0f
	);
	
	FVector SpawnLocation = PlayerLocation + RandomOffset;
	
	// 지면에 맞춤
	FHitResult HitResult;
	FVector StartTrace = SpawnLocation + FVector(0, 0, 1000.0f);
	FVector EndTrace = SpawnLocation - FVector(0, 0, 1000.0f);
	
	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartTrace, EndTrace, ECC_WorldStatic))
	{
		SpawnLocation.Z = HitResult.Location.Z;
	}
	
	return SpawnLocation;
}

FVector APGStageManager::GetSafeSpawnLocation() const
{
	// 플레이어 위치 기준
	FVector PlayerLocation = GetActorLocation();
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		PlayerLocation = PlayerPawn->GetActorLocation();
	}
	
	// 랜덤 원형 위치 생성
	float RandomAngle = SpawnRandom.FRandRange(0.0f, 360.0f);
	float RandomRadius = SpawnRandom.FRandRange(CurrentStageDataCache.SpawnRadius * 0.7f, CurrentStageDataCache.SpawnRadius);
	
	FVector RandomOffset = FVector(
		FMath::Cos(FMath::DegreesToRadians(RandomAngle)) * RandomRadius,
		FMath::Sin(FMath::DegreesToRadians(RandomAngle)) * RandomRadius,
		0.0f
	);
	
	FVector TargetLocation = PlayerLocation + RandomOffset;
	
	// 내비게이션 시스템을 통한 안전한 위치 찾기
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (NavSys)
	{
		FNavLocation NavLocation;
		// 반경 500 유닛 내에서 네비게이션 가능한 위치 찾기
		if (NavSys->ProjectPointToNavigation(TargetLocation, NavLocation, FVector(500.0f, 500.0f, 1000.0f)))
		{
			return NavLocation.Location;
		}
	}
	
	// 내비게이션 실패 시 일반적인 지면 체크
	FHitResult HitResult;
	FVector StartTrace = TargetLocation + FVector(0, 0, 1000.0f);
	FVector EndTrace = TargetLocation - FVector(0, 0, 1000.0f);
	
	// 지면 체크를 위해 Static / Dynamic 모두 검사
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	
	if (UKismetSystemLibrary::LineTraceSingleForObjects(
		GetWorld(), StartTrace, EndTrace, ObjectTypes, false, TArray<AActor*>(), 
		EDrawDebugTrace::None, HitResult, true))
	{
		return HitResult.Location + FVector(0, 0, 5.0f); // 지면에서 살짝 위로
	}
	
	return TargetLocation;
}

bool APGStageManager::IsValidSpawnLocation(const FVector& Location, float CapsuleRadius, float CapsuleHalfHeight, EPGSpawnFailure& OutFailure) const
{
	OutFailure = EPGSpawnFailure::None;
	// 1. 경사면 각도 체크
	if (!IsValidSlope(Location, OutFailure))
	{
		return false;
	}
	
	// 2. 내비게이션 메시 체크
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys) { OutFailure = EPGSpawnFailure::NavigationSystem; return false; }
	if (!NavSys->GetDefaultNavDataInstance()) { OutFailure = EPGSpawnFailure::NavigationData; return false; }
	if (NavSys)
	{
		FNavLocation NavLocation;
		if (!NavSys->ProjectPointToNavigation(Location, NavLocation, FVector(100.0f, 100.0f, 200.0f)))
		{
			OutFailure = EPGSpawnFailure::NavigationProjection;
			return false;
		}
	}

    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) { OutFailure = EPGSpawnFailure::Player; return false; }
    UNavigationPath* Path = UNavigationSystemV1::FindPathToActorSynchronously(
        GetWorld(), Location, Player, 50.f);
    if (!Path || !Path->IsValid() || Path->IsPartial()) { OutFailure = EPGSpawnFailure::Path; return false; }
	
	// 3. 충돌 체크 (캐릭터 크기의 캡슐로 체크)
	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	
	if (GetWorld()->OverlapBlockingTestByChannel(
		Location, FQuat::Identity, ECC_Pawn, CapsuleShape, QueryParams))
	{
		OutFailure = EPGSpawnFailure::Capsule;
		return false;
	}
	
	// 4. 플레이어와 최소 거리 체크
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		float DistanceToPlayer = FVector::Dist(Location, PlayerPawn->GetActorLocation());
		const float MinDistanceFromPlayer = 200.0f; // 최소 거리
		
		if (DistanceToPlayer < MinDistanceFromPlayer)
		{
			OutFailure = EPGSpawnFailure::PlayerDistance;
			return false;
		}
	}
	
	return true;
}

bool APGStageManager::IsValidSlope(const FVector& Location, EPGSpawnFailure& OutFailure) const
{
    FHitResult Ground;
    FCollisionObjectQueryParams GroundObjects;
    GroundObjects.AddObjectTypesToQuery(ECC_WorldStatic);
    GroundObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
    if (!GetWorld()->LineTraceSingleByObjectType(Ground, Location + FVector(0,0,50), Location - FVector(0,0,1000), GroundObjects))
    { OutFailure = EPGSpawnFailure::Ground; return false; }
	// 여러 방향으로 레이캐스트하여 경사면 체크
	TArray<FVector> CheckDirections = {
		FVector(1, 0, 0),   // 동쪽
		FVector(-1, 0, 0),  // 서쪽  
		FVector(0, 1, 0),   // 북쪽
		FVector(0, -1, 0)   // 남쪽
	};
	
	const float CheckDistance = 100.0f;
	const float MaxSlopeAngle = 45.0f; // 최대 경사각 (도)
	
	for (const FVector& Direction : CheckDirections)
	{
		FVector StartPos = Location + FVector(0, 0, 50.0f);
		FVector EndPos = StartPos + (Direction * CheckDistance);
		EndPos.Z -= 200.0f; // 아래쪽으로 체크
		
		FHitResult HitResult;
		if (GetWorld()->LineTraceSingleByChannel(HitResult, StartPos, EndPos, ECC_WorldStatic))
		{
			FVector SurfaceNormal = HitResult.Normal;
			float SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(SurfaceNormal.Z, -1., 1.)));
			
			if (SlopeAngle > MaxSlopeAngle)
			{
				OutFailure = EPGSpawnFailure::Slope;
				return false; // 너무 가파른 경사
			}
		}
	}
	
	return true;
}

void APGStageManager::LogSpawnFailure(int32 EnemyId, int32 Attempt, EPGSpawnFailure Reason, const FVector& Location) const
{
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    const auto* Recast = Nav ? Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance()) : nullptr;
    UE_LOG(LogTemp, Log, TEXT("PGSpawn rejected stage=%d wave=%d enemy=%d attempt=%d retry=%d/%d reason=%s location=%s world=%s type=%d nav=%s bounds=%s building=%d runtime=%d"),
        CurrentStageId, GetCurrentWaveNumber(), EnemyId, Attempt, SpawnFailureCount + 1, CurrentStageDataCache.MaxSpawnRetries,
        PGSpawnFailureName(Reason), *Location.ToString(), *GetWorld()->GetName(), int32(GetWorld()->WorldType), *GetNameSafe(Recast),
        Recast ? *Recast->GetNavMeshBounds().ToString() : TEXT("none"), Nav && Nav->IsNavigationBuildInProgress(),
        Recast ? int32(Recast->GetRuntimeGenerationMode()) : -1);
    if (CVarPGSpawnDebug.GetValueOnGameThread())
    {
        DrawDebugPoint(GetWorld(), Location, 15.f, FColor::Red, false, 5.f);
        DrawDebugString(GetWorld(), Location, FString::Printf(TEXT("%d: %s (%d)"), EnemyId, PGSpawnFailureName(Reason), Attempt), nullptr, FColor::Red, 5.f);
    }
}

void APGStageManager::OnEnemyKilled(APGCharacterEnemy* KilledEnemy)
{
    if (CurrentStageState != EPGStageState::InProgress || !KilledEnemy || SpawnedEnemies.Remove(KilledEnemy) == 0) return;
    KilledEnemy->OnDestroyed.RemoveDynamic(this, &ThisClass::OnTrackedEnemyDestroyed);
    if (KilledEnemy->GetPGAbilitySystemComponent()->GetHealth() <= 0)
        if (auto* Tables = UPGDataTableManager::Get(this))
            if (const auto* Row = Tables->GetRowData<FPGEnemyDataRow>(KilledEnemy->GetCharacterTID()); Row && Row->Role == EPGEnemyRole::Boss)
                BossDefeatPresentationUntil = GetWorld()->GetTimeSeconds() + FMath::Clamp(Row->DefeatDisplaySeconds, 0.f, 10.f);
    RemainingMonsters = FMath::Max(0, RemainingMonsters - 1);
    OnMonsterCountChanged.Broadcast(RemainingMonsters);
    CheckStageComplete();
}

void APGStageManager::ShowRewardSelection()
{
    OfferedRewards.Reset();
    for (const FPGStageReward& Reward : CurrentStageDataCache.RewardPool)
    {
        // Item/skill ownership arrives in the farming milestone. Never silently grant unsupported rewards.
        if (Reward.RewardType != EPGRewardType::Stat || !PGData()->GetRowData<FPGRewardStatDataRow>(Reward.RewardId))
        {
            FailStage(TEXT("Reward pool contains an unsupported or missing reward. Configure Stat rewards for this milestone."));
            return;
        }
    }
      TArray<FPGStageReward> Pool = CurrentStageDataCache.RewardPool;
    if (const auto* Profile = UPGProfileSubsystem::Get(this))
        Pool.RemoveAll([&](const FPGStageReward& Entry)
        {
            const auto* Reward = PGData()->GetRowData<FPGRewardStatDataRow>(Entry.RewardId);
              if (!Reward) return true;
              return !Profile->IsRewardEligible(*Reward);
          });
      if (const auto* Profile = UPGProfileSubsystem::Get(this))
          for (auto& Entry : Pool)
          {
              const auto* Reward = PGData()->GetRowData<FPGRewardStatDataRow>(Entry.RewardId);
              if (!Reward || Reward->BuildFamily.IsNone()) continue;
              for (auto Chosen : Profile->GetProfile()->SelectedRewards)
                  if (const auto* Previous = PGData()->GetRowData<FPGRewardStatDataRow>(Chosen.Key))
                      if (Previous->BuildFamily == Reward->BuildFamily) { Entry.Weight *= 2.f; break; }
          }
    const auto* RewardProfile = UPGProfileSubsystem::Get(this);
    const int32 ChoiceNumber = RewardProfile ? RewardProfile->GetProfile()->StageRewardCounts.FindRef(CurrentStageId) : 0;
    FRandomStream RewardRandom(PGRunRandom::Seed(RunSeed, CurrentStageId, 0, 2, ChoiceNumber));
    OfferedRewards = PGRewardSelection::Draw(MoveTemp(Pool), CurrentStageDataCache.bReserveKeystoneChoice,
        [this](int32 Id) { const auto* Row = PGData()->GetRowData<FPGRewardStatDataRow>(Id); return Row && Row->bKeystone; }, RewardRandom);
    RewardToken = FGuid::NewGuid();
    for (const auto& Reward : CurrentStageDataCache.RewardPool)
        UE_LOG(LogTemp, Log, TEXT("PGReward pool stage=%d id=%d weight=%.2f"), CurrentStageId, Reward.RewardId, Reward.Weight);
    UE_LOG(LogTemp, Log, TEXT("PGReward offered stage=%d token=%s count=%d"), CurrentStageId, *RewardToken.ToString(), OfferedRewards.Num());
    FPGStagePresentation View;
    View.Owner = this; View.Token = RewardToken; View.Choices = OfferedRewards;
    View.Submit.BindUObject(this, &ThisClass::CommitReward);
    if (auto* Messages = UPGMessageManager::Get(this)) Messages->SendMessage(EPGUIMessageType::StagePresentation, &View);
}

void APGStageManager::OnRewardSelected()
{
    if (CurrentStageState != EPGStageState::BuildPhase || !bRewardCommitted) return;
    CloseRewardWindow();
}

void APGStageManager::GoToNextStage()
{
    if (CurrentStageState != EPGStageState::Completed) return;
    if (PGData() && PGData()->GetRowData<FPGStageDataRow>(CurrentStageId + 1)) StartStage(CurrentStageId + 1);
    else
    {
          CurrentStageState = EPGStageState::Finished;
          if (auto* Profile = UPGProfileSubsystem::Get(this))
              if (!Profile->EndRun(true, CurrentStageId)) { FailStage(TEXT("클리어 기록 저장 실패. 다시 시도해 주세요.")); return; }
        OnRunFinished.Broadcast();
        // Victory is already committed. Leave the boss defeat visible before covering gameplay with results.
        const float Delay = FMath::Max(0.f, float(BossDefeatPresentationUntil - GetWorld()->GetTimeSeconds()));
        if (Delay > 0.f) GetWorldTimerManager().SetTimer(BossResultTimer, this, &ThisClass::ShowRunResult, Delay, false);
        else ShowRunResult();
    }
}

void APGStageManager::ShowRunResult()
{
    if (CurrentStageState != EPGStageState::Finished) return;
    const auto* Profile = UPGProfileSubsystem::Get(this);
    ShowStageStatus(FText::FromString(FString::Printf(TEXT("시련 돌파!\n완료 구간 %d · 선택한 강화 %d종\n장비와 강화는 새 도전에서 초기화됩니다"),
        CurrentStageId, Profile ? Profile->GetProfile()->SelectedRewards.Num() : 0)));
}

void APGStageManager::StartNextStageAfterDelay()
{
	FinishBuildPhase();
}

void APGStageManager::OnActorDied(const IPGEventData* InEventData)
{
    if (!InEventData) return;
    const auto* Data = static_cast<const FPGEventDataOneParam<TWeakObjectPtr<APGCharacterEnemy>>*>(InEventData);
    if (Data->Value.IsValid()) OnEnemyKilled(Data->Value.Get());
}

void APGStageManager::OnActorSpawned(const IPGEventData* InEventData)
{
    if (CurrentStageState != EPGStageState::InProgress || !CurrentStageDataCache.bCountSummonedEnemies || !InEventData) return;
    const auto* Data = static_cast<const FPGEventDataOneParam<TWeakObjectPtr<APGCharacterEnemy>>*>(InEventData);
    APGCharacterEnemy* Enemy = Data->Value.Get();
    if (!IsValid(Enemy) || Enemy->GetWorld() != GetWorld() || SpawnedEnemies.Contains(Enemy)) return;
    SpawnedEnemies.Add(Enemy);
    Enemy->OnDestroyed.AddUniqueDynamic(this, &ThisClass::OnTrackedEnemyDestroyed);
    ++RemainingMonsters;
    OnMonsterCountChanged.Broadcast(RemainingMonsters);
}

void APGStageManager::ClearAllEnemies()
{
    const auto Enemies = SpawnedEnemies;
    SpawnedEnemies.Empty();
    for (APGCharacterEnemy* Enemy : Enemies)
        if (IsValid(Enemy))
        {
            Enemy->OnDestroyed.RemoveDynamic(this, &ThisClass::OnTrackedEnemyDestroyed);
            Enemy->Destroy();
        }
}

int32 APGStageManager::GetRemainingSpawnCountForMonsterType(int32 MonsterId) const
{
	for (const FPGMonsterSpawnQueueItem& QueueItem : MonsterSpawnQueue)
	{
		if (QueueItem.MonsterId == MonsterId)
		{
			return QueueItem.RemainingCount;
		}
	}
	return 0;
}

int32 APGStageManager::GetTotalMonsterCount(const FPGStageDataRow& StageData)
{
	int32 TotalCount = 0;
    if (StageData.Waves.IsEmpty())
        for (const auto& Spawn : StageData.MonsterSpawnInfos) TotalCount += Spawn.SpawnCount;
    else for (const auto& Wave : StageData.Waves)
        for (const auto& Spawn : Wave.MonsterSpawnInfos) TotalCount += Spawn.SpawnCount;
	return TotalCount;
}

bool APGStageManager::IsValidStageData(const FPGStageDataRow& StageData)
{
    FString Error;
    const bool bValid = PGStageValidation::Validate(StageData, Error);
    if (!bValid) UE_LOG(LogTemp, Error, TEXT("Stage %d: %s"), StageData.Id, *Error);
    return bValid;
}

FPGStageDataRow APGStageManager::GetCurrentStageDataCopy() const
{
	return CurrentStageDataCache;
}

void APGStageManager::FailStage(const FString& Reason)
{
    if (auto* Telemetry = UPGRunTelemetrySubsystem::Get(this)) Telemetry->EndStage(Reason);
    CurrentStageState = EPGStageState::Failed;
    GetWorldTimerManager().ClearAllTimersForObject(this);
    RewardToken.Invalidate();
    CloseRewardWindow();
    UE_LOG(LogTemp, Error, TEXT("Stage %d failed: %s"), CurrentStageId, *Reason);
    if (auto* PC = Cast<APGPlayerController>(UGameplayStatics::GetPlayerController(this, 0))) PC->CloseInventory();
    ShowStageStatus(FText::Format(NSLOCTEXT("PG", "StageFailed", "진행을 중단했습니다\n{0}"), FText::FromString(Reason)));
    OnStageFailed.Broadcast(Reason);
}

void APGStageManager::CloseRewardWindow()
{
    FPGStagePresentation View; View.Owner = this; View.bClose = true;
    if (auto* Messages = UPGMessageManager::Get(this)) Messages->SendMessage(EPGUIMessageType::StagePresentation, &View);
}

void APGStageManager::CheckStageComplete()
{
    if (CurrentStageState == EPGStageState::InProgress && RemainingMonsters == 0 && MonsterSpawnQueue.IsEmpty() && SpawnedEnemies.IsEmpty())
    {
        if (auto* Telemetry = UPGRunTelemetrySubsystem::Get(this)) Telemetry->EndWave();
        GetWorldTimerManager().ClearTimer(SpawnTimer);
        if (ActiveWaves.IsValidIndex(CurrentWaveIndex + 1)) PrepareWave(CurrentWaveIndex + 1);
        else BeginBuildPhase();
    }
}

void APGStageManager::BeginBuildPhase()
{
    if (auto* Telemetry = UPGRunTelemetrySubsystem::Get(this)) Telemetry->EndStage(TEXT("Cleared"));
    if (CurrentStageDataCache.bIsBossStage && CurrentStageDataCache.bManualReady && CurrentStageDataCache.RewardPool.IsEmpty())
    {
        OnAllMonstersKilled.Broadcast(); OnStageCompleted.Broadcast(CurrentStageId);
        CurrentStageState = EPGStageState::Completed; GoToNextStage(); return;
    }
    CurrentStageState = EPGStageState::BuildPhase;
    RewardsRemaining = FMath::Clamp(CurrentStageDataCache.RewardSelections, 1, 3);
    if (auto* Profile = UPGProfileSubsystem::Get(this)) RewardsRemaining = FMath::Max(1, RewardsRemaining - Profile->GetProfile()->StageRewardCounts.FindRef(CurrentStageId));
    if (!CurrentStageDataCache.bManualReady)
        GetWorldTimerManager().SetTimer(NextStageTimer, this, &ThisClass::FinishBuildPhase,
            FMath::Max(.01f, CurrentStageDataCache.BuildDuration), false);
    OnAllMonstersKilled.Broadcast();
    OnStageCompleted.Broadcast(CurrentStageId);
    ShowRewardSelection();
}
void APGStageManager::ReadyForNextStage()
{
    if (CurrentStageState == EPGStageState::RunPreparation) { StartStage(CurrentStageId); return; }
    if (CanReady()) FinishBuildPhase();
}
void APGStageManager::PrepareRun(int32 StageId)
{
    GetWorldTimerManager().ClearTimer(BossResultTimer);
    BossDefeatPresentationUntil = 0;
    CurrentStageId = StageId;
    CurrentStageState = EPGStageState::RunPreparation;
}

void APGStageManager::FinishBuildPhase()
{
    if (CurrentStageState != EPGStageState::BuildPhase) return;
    if (!bRewardCommitted && !CommitReward(RewardToken, OfferedRewards.IsEmpty() ? INDEX_NONE : 0))
    {
        FailStage(TEXT("Could not save the stage reward. Retry from the checkpoint."));
        return;
    }
    GetWorldTimerManager().ClearTimer(NextStageTimer);
    CloseRewardWindow();
    if (auto* PC = Cast<APGPlayerController>(UGameplayStatics::GetPlayerController(this, 0))) PC->CloseInventory();
    CurrentStageState = EPGStageState::Completed;
    GoToNextStage();
}

float APGStageManager::GetBuildTimeRemaining() const
{
    return CurrentStageState == EPGStageState::BuildPhase ? FMath::Max(0.f, GetWorldTimerManager().GetTimerRemaining(NextStageTimer)) : 0.f;
}

float APGStageManager::GetWaveTimeRemaining() const
{
    return CurrentStageState == EPGStageState::WaveIntermission ? FMath::Max(0.f, GetWorldTimerManager().GetTimerRemaining(WaveTimer)) : 0.f;
}

bool APGStageManager::CommitReward(FGuid Token, int32 Choice)
{
    UE_LOG(LogTemp, Log, TEXT("PGReward submit stage=%d token=%s choice=%d"), CurrentStageId, *Token.ToString(), Choice);
    if (CurrentStageState != EPGStageState::BuildPhase || bRewardCommitted || !Token.IsValid() || Token != RewardToken) return false;
    APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Player || !Player->GetStatComponent() || Player->GetStatComponent()->GetCurrentHealth() <= 0.f) return false;
    EPGStatType RewardStat = EPGStatType::None;
    int32 RewardAmount = 0, PerkPercent = 0;
    EPGCombatPerk Perk = EPGCombatPerk::None;
    if (!OfferedRewards.IsEmpty())
    {
        if (!OfferedRewards.IsValidIndex(Choice)) return false;
        const FPGRewardStatDataRow* Reward = PGData()->GetRowData<FPGRewardStatDataRow>(OfferedRewards[Choice].RewardId);
        if (!Reward) return false;
        RewardStat = Reward->StatType; RewardAmount = Reward->Amount; Perk = Reward->Perk; PerkPercent = Reward->PerkPercent;
    }
    else if (Choice != INDEX_NONE) return false;
    if (auto* Profile = UPGProfileSubsystem::Get(this))
    {
        if (!Profile->CommitReward(Token, CurrentStageId + 1, RewardStat, RewardAmount, Perk, PerkPercent,
            OfferedRewards.IsValidIndex(Choice) ? OfferedRewards[Choice].RewardId : 0, RewardsRemaining <= 1)) return false;
    }
    else if (Perk != EPGCombatPerk::None) return false;
    else if (RewardAmount > 0 && !Player->GetStatComponent()->ApplyStatReward(RewardStat, RewardAmount)) return false;
    const int32 RewardId = OfferedRewards.IsValidIndex(Choice) ? OfferedRewards[Choice].RewardId : 0;
    if (auto* Telemetry = UPGRunTelemetrySubsystem::Get(this)) Telemetry->RecordReward(RewardId);
    UE_LOG(LogTemp, Log, TEXT("PGRun reward stage=%d seed=%d id=%d"), CurrentStageId, RunSeed, RewardId);
    if (--RewardsRemaining > 0)
    {
        RewardToken.Invalidate();
        CloseRewardWindow();
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
        { if (CurrentStageState == EPGStageState::BuildPhase) ShowRewardSelection(); }));
        return true;
    }
    bRewardCommitted = true;
    UE_LOG(LogTemp, Log, TEXT("PGReward applied stage=%d stat=%d amount=%d"), CurrentStageId, static_cast<int32>(RewardStat), RewardAmount);
    RewardToken.Invalidate();
    OnRewardSelected();
    return true;
}

void APGStageManager::OnTrackedEnemyDestroyed(AActor* Actor)
{
    OnEnemyKilled(Cast<APGCharacterEnemy>(Actor));
}
void APGStageManager::OnPlayerDied(const IPGEventData* Data)
{
    if (auto* Profile = UPGProfileSubsystem::Get(this)) Profile->EndRun(false, CurrentStageId);
    if (CurrentStageState == EPGStageState::InProgress || CurrentStageState == EPGStageState::WaveIntermission || CurrentStageState == EPGStageState::BuildPhase || CurrentStageState == EPGStageState::Completed)
        FailStage(TEXT("Player defeated."));
}
void APGStageManager::ShowStageStatus(const FText& Text)
{
    FPGStagePresentation View; View.Owner = this; View.Status = Text;
    View.Retry.BindUObject(this, &ThisClass::RestartRun);
    if (auto* Messages = UPGMessageManager::Get(this)) Messages->SendMessage(EPGUIMessageType::StagePresentation, &View);
}
void APGStageManager::RestartRun()
{
    if (CurrentStageState == EPGStageState::Finished ||
        (UPGProfileSubsystem::Get(this) && UPGProfileSubsystem::Get(this)->GetCatalog() && UPGProfileSubsystem::Get(this)->GetCatalog()->bRoguelikeRuns))
        if (auto* Profile = UPGProfileSubsystem::Get(this)) if (!Profile->BeginNewRun()) return;
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
