#include "PGStageManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/NavigationSystemBase.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataTemplate.h"

APGStageManager::APGStageManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void APGStageManager::BeginPlay()
{
	Super::BeginPlay();

	OnActorDiedHandle = PGMessage()->RegisterDelegate(EPGSharedMessageType::OnDied,
	this, &ThisClass::OnActorDied);

	OnActorDiedHandle = PGMessage()->RegisterDelegate(EPGSharedMessageType::OnSpawned,
	this, &ThisClass::OnActorSpawned);
}

void APGStageManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OnActorDiedHandle.IsValid())
	{
		if (UPGMessageManager* Manager = PGMessage())
		{
			Manager->UnregisterDelegate(EPGSharedMessageType::OnDied, OnActorDiedHandle);
			Manager->UnregisterDelegate(EPGSharedMessageType::OnSpawned, OnActorSpawnedHandle);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void APGStageManager::StartStage(int32 StageId)
{
	// 다음 스테이지로 자동 진행하는 경우
	if (StageId == -1)
	{
		StageId = CurrentStageId + 1;
	}
	
	// 스테이지 데이터 로드
	if (!LoadStageData(StageId))
	{
		return;
	}
	if (!IsValidStageData(CurrentStageDataCache))
	{
		UE_LOG(LogTemp, Error, TEXT("PGStageManagerImproved: 스테이지 %d 데이터가 유효하지 않음"), StageId);
		return;
	}
	
	// 기존 타이머 정리
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
	GetWorld()->GetTimerManager().ClearTimer(NextStageTimer);
	
	// 기존 적들 제거
	ClearAllEnemies();
	
	// 스테이지 설정
	CurrentStageId = StageId;
	const int32 TotalMonsterCount = GetTotalMonsterCount(CurrentStageDataCache);
	RemainingMonsters = TotalMonsterCount;
	SpawnedMonsters = 0;
	CurrentStageState = EPGStageState::InProgress;

	InitializeMonsterSpawnQueue();
	
	// 델리게이트 호출
	OnStageStarted.Broadcast(CurrentStageId);
	OnMonsterCountChanged.Broadcast(RemainingMonsters);

	// 메시지 전송
	FPGEventDataOneParam<int32> ToSendData(StageId);
	UPGMessageManager::Get()->SendMessage(EPGUIMessageType::StageChanged, &ToSendData);
	
	UE_LOG(LogTemp, Log, TEXT("PGStageManagerImproved: 스테이지 %d 시작 - 총 몬스터 %d마리 (타입 %d가지)"), 
		   CurrentStageId, TotalMonsterCount, CurrentStageDataCache.MonsterSpawnInfos.Num());
    
	// 몬스터 스폰 시작
	if (CurrentStageDataCache.SpawnInterval > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(SpawnTimer, 
			this, &APGStageManager::SpawnEnemyBatch, 
			CurrentStageDataCache.SpawnInterval, true, 0.0f);
	}
	else
	{
		// 즉시 모든 몬스터 스폰
		const int32 TotalCount = GetTotalMonsterCount(CurrentStageDataCache);
		while (SpawnedMonsters < TotalCount && !MonsterSpawnQueue.IsEmpty())
		{
			SpawnEnemyBatch();
		}
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
	for (const FPGMonsterSpawnInfo& SpawnInfo : CurrentStageDataCache.MonsterSpawnInfos)
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
	if (CurrentStageState != EPGStageState::InProgress)
	{
		return;
	}
	
	if (MonsterSpawnQueue.IsEmpty())
	{
		// 모든 몬스터 스폰 완료
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
		return;
	}

	// 스폰할 몬스터 수 계산
	const int32 TotalRemaining = GetTotalMonsterCount(CurrentStageDataCache) - SpawnedMonsters;
	const int32 MonstersToSpawn = FMath::Min(CurrentStageDataCache.SpawnBatchSize, TotalRemaining);
    
	int32 SpawnedInThisBatch = 0;
    
	// 배치 스폰
	for (int32 i = 0; i < MonstersToSpawn && !MonsterSpawnQueue.IsEmpty(); i++)
	{
		int32 SelectedMonsterId = SelectNextMonsterToSpawn();
		if (SelectedMonsterId <= 0)
		{
			break;
		}
        
		APGCharacterEnemy* SpawnedEnemy = SpawnSingleEnemy(SelectedMonsterId);
		if (SpawnedEnemy)
		{
			SpawnedMonsters++;
			SpawnedInThisBatch++;
			SpawnedEnemies.Add(SpawnedEnemy);
            
			// 몬스터 타입별 스폰 알림
			const int32 RemainingOfThisType = GetRemainingSpawnCountForMonsterType(SelectedMonsterId);
			OnMonsterTypeSpawned.Broadcast(SelectedMonsterId, RemainingOfThisType);
		}
	}
    
	UE_LOG(LogTemp, Log, TEXT("PGStageManagerImproved: %d마리 몬스터 스폰됨 (총 %d/%d)"), 
		   SpawnedInThisBatch, SpawnedMonsters, GetTotalMonsterCount(CurrentStageDataCache));
    
	// 모든 몬스터 스폰 완료 시 타이머 정지
	if (SpawnedMonsters >= GetTotalMonsterCount(CurrentStageDataCache) || MonsterSpawnQueue.IsEmpty())
	{
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
	}
}

APGCharacterEnemy* APGStageManager::SpawnSingleEnemy(int32 EnemyId)
{
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
	FVector SpawnLocation = FVector::ZeroVector;
	bool bFoundValidLocation = false;
	
	for (int32 Attempt = 0; Attempt < 5; ++Attempt)
	{
		SpawnLocation = GetSafeSpawnLocation();
		if (IsValidSpawnLocation(SpawnLocation))
		{
			bFoundValidLocation = true;
			break;
		}
	}
	
	// 유효한 위치를 찾지 못한 경우 기본 위치 사용
	if (!bFoundValidLocation)
	{
		SpawnLocation = GetRandomSpawnLocation();
	}
	
	FRotator SpawnRotation = FRotator::ZeroRotator;
	
	// 적 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	
	APGCharacterEnemy* SpawnedEnemy = GetWorld()->SpawnActor<APGCharacterEnemy>(
		EnemyData->ActorClass.LoadSynchronous(), SpawnLocation, SpawnRotation, SpawnParams);
	
	if (SpawnedEnemy)
	{
		// 델리게이트 호출
		OnEnemySpawned.Broadcast(SpawnedEnemy);
	}
	
	return SpawnedEnemy;
}

int32 APGStageManager::SelectNextMonsterToSpawn()
{
	if (MonsterSpawnQueue.IsEmpty())
	{
		return 0;
	}
    
	// 현재 시간 기준으로 스폰 가능한 몬스터 찾기
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float StageStartTime = CurrentTime - (SpawnedMonsters * CurrentStageDataCache.SpawnInterval);
    
	// 우선순위가 높고 딜레이 시간이 지난 몬스터 중에서 선택
	for (int32 i = 0; i < MonsterSpawnQueue.Num(); i++)
	{
		FPGMonsterSpawnQueueItem& QueueItem = MonsterSpawnQueue[i];
        
		// 딜레이 시간 체크
		if (CurrentTime >= StageStartTime + QueueItem.DelayTime)
		{
			// 이 타입에서 하나 스폰
			QueueItem.RemainingCount--;
			const int32 SelectedMonsterId = QueueItem.MonsterId;
            
			// 남은 수량이 0이면 대기열에서 제거
			if (QueueItem.RemainingCount <= 0)
			{
				MonsterSpawnQueue.RemoveAt(i);
			}
            
			return SelectedMonsterId;
		}
	}
    
	// 딜레이 시간이 지나지 않은 경우 0 반환 (스폰하지 않음)
	return 0;
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
	float RandomAngle = FMath::RandRange(0.0f, 360.0f);
	float RandomRadius = FMath::RandRange(CurrentStageDataCache.SpawnRadius * 0.7f, CurrentStageDataCache.SpawnRadius);
	
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
	float RandomAngle = FMath::RandRange(0.0f, 360.0f);
	float RandomRadius = FMath::RandRange(CurrentStageDataCache.SpawnRadius * 0.7f, CurrentStageDataCache.SpawnRadius);
	
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

bool APGStageManager::IsValidSpawnLocation(const FVector& Location) const
{
	// 1. 경사면 각도 체크
	if (!IsValidSlope(Location))
	{
		return false;
	}
	
	// 2. 내비게이션 메시 체크
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (NavSys)
	{
		FNavLocation NavLocation;
		if (!NavSys->ProjectPointToNavigation(Location, NavLocation, FVector(100.0f, 100.0f, 200.0f)))
		{
			return false;
		}
	}
	
	// 3. 충돌 체크 (캐릭터 크기의 캡슐로 체크)
	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(50.0f, 100.0f);
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	
	if (GetWorld()->OverlapBlockingTestByChannel(
		Location, FQuat::Identity, ECC_Pawn, CapsuleShape, QueryParams))
	{
		return false;
	}
	
	// 4. 플레이어와 최소 거리 체크
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		float DistanceToPlayer = FVector::Dist(Location, PlayerPawn->GetActorLocation());
		const float MinDistanceFromPlayer = 200.0f; // 최소 거리
		
		if (DistanceToPlayer < MinDistanceFromPlayer)
		{
			return false;
		}
	}
	
	return true;
}

bool APGStageManager::IsValidSlope(const FVector& Location) const
{
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
			float SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(SurfaceNormal.Z));
			
			if (SlopeAngle > MaxSlopeAngle)
			{
				return false; // 너무 가파른 경사
			}
		}
	}
	
	return true;
}

void APGStageManager::OnEnemyKilled(APGCharacterEnemy* KilledEnemy)
{

}

void APGStageManager::ShowRewardSelection()
{
	// TODO: 보상 선택 UI 표시
	
	// 테스트용: {}초 후 자동으로 보상 선택 완료
	FTimerHandle AutoRewardTimer;
	GetWorld()->GetTimerManager().SetTimer(AutoRewardTimer, 
		this, &APGStageManager::OnRewardSelected, 1.5f, false);
}

void APGStageManager::OnRewardSelected()
{
	if (CurrentStageState != EPGStageState::RewardPhase)
	{
		return;
	}
	
	CurrentStageState = EPGStageState::Completed;
	OnStageCompleted.Broadcast(CurrentStageId);
	
	// 다음 스테이지로 진행
	GetWorld()->GetTimerManager().SetTimer(NextStageTimer, 
		this, &APGStageManager::StartNextStageAfterDelay, 
		CurrentStageDataCache.NextStageDelay, false);
}

void APGStageManager::GoToNextStage()
{
	// 다음 스테이지 데이터 확인
	if (LoadStageData(CurrentStageId + 1))
	{
		StartStage(CurrentStageId + 1);
	}
	else
	{
		// TODO: 게임 완료 처리
	}
}

void APGStageManager::StartNextStageAfterDelay()
{
	GoToNextStage();
}

void APGStageManager::OnActorDied(const IPGEventData* InEventData)
{
	if (CurrentStageState != EPGStageState::InProgress)
	{
		return;
	}

	const FPGEventDataOneParam<TWeakObjectPtr<APGCharacterEnemy>>* CastedParam
		= static_cast<const FPGEventDataOneParam<TWeakObjectPtr<APGCharacterEnemy>>*>(InEventData);
	if (nullptr == CastedParam || false == CastedParam->Value.IsValid())

	// 스폰된 적 목록에서 제거
	SpawnedEnemies.Remove(CastedParam->Value.Get());
	
	RemainingMonsters = FMath::Max(0, RemainingMonsters - 1);
	OnMonsterCountChanged.Broadcast(RemainingMonsters);
	
	UE_LOG(LogTemp, Log, TEXT("PGStageManager: 적 처치됨. 남은 적: %d"), RemainingMonsters);
	
	// 모든 몬스터 처치 완료
	if (RemainingMonsters <= 0)
	{
		CurrentStageState = EPGStageState::RewardPhase;
		OnAllMonstersKilled.Broadcast();
		ShowRewardSelection();
		
		UE_LOG(LogTemp, Log, TEXT("PGStageManager: 스테이지 %d 클리어! 보상 선택 단계"), CurrentStageId);
	}
}

void APGStageManager::OnActorSpawned(const IPGEventData* InEventData)
{
	const FPGEventDataOneParam<TWeakObjectPtr<APGCharacterEnemy>>* CastedParam
		= static_cast<const FPGEventDataOneParam<TWeakObjectPtr<APGCharacterEnemy>>*>(InEventData);
	if (nullptr == CastedParam || false == CastedParam->Value.IsValid())
	{
		return;
	}

	SpawnedEnemies.Add(CastedParam->Value.Get());
	++RemainingMonsters;
}

void APGStageManager::ClearAllEnemies()
{
	for (APGCharacterEnemy* Enemy : SpawnedEnemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->Destroy();
		}
	}
	SpawnedEnemies.Empty();
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
	for (const FPGMonsterSpawnInfo& SpawnInfo : StageData.MonsterSpawnInfos)
	{
		TotalCount += SpawnInfo.SpawnCount;
	}
	return TotalCount;
}

bool APGStageManager::IsValidStageData(const FPGStageDataRow& StageData)
{
	if (StageData.MonsterSpawnInfos.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("스테이지 데이터 검증 실패: MonsterSpawnInfos가 비어있음"));
		return false;
	}

	for (int32 i = 0; i < StageData.MonsterSpawnInfos.Num(); i++)
	{
		const FPGMonsterSpawnInfo& SpawnInfo = StageData.MonsterSpawnInfos[i];
        
		if (SpawnInfo.MonsterId <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("스테이지 데이터 검증 실패: 인덱스 %d - 유효하지 않은 몬스터 ID: %d"), 
				i, SpawnInfo.MonsterId);
			return false;
		}
        
		if (SpawnInfo.SpawnCount <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("스테이지 데이터 검증 실패: 인덱스 %d - 유효하지 않은 스폰 수량: %d"), 
				i, SpawnInfo.SpawnCount);
			return false;
		}
        
		if (SpawnInfo.SpawnDelayTime < 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("스테이지 데이터 검증 실패: 인덱스 %d - 유효하지 않은 딜레이 시간: %.2f"), 
				i, SpawnInfo.SpawnDelayTime);
			return false;
		}
	}

	return true;
}

FPGStageDataRow APGStageManager::GetCurrentStageDataCopy() const
{
	return CurrentStageDataCache;
}
