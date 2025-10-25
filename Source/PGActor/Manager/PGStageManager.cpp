#include "PGStageManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
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
}

void APGStageManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OnActorDiedHandle.IsValid())
	{
		if (UPGMessageManager* Manager = PGMessage())
		{
			Manager->UnregisterDelegate(EPGSharedMessageType::OnDied, OnActorDiedHandle);
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
	
	// 기존 타이머 정리
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
	GetWorld()->GetTimerManager().ClearTimer(NextStageTimer);
	
	// 기존 적들 제거
	ClearAllEnemies();
	
	// 스테이지 설정
	CurrentStageId = StageId;
	RemainingMonsters = CurrentStageDataCache.TotalMonsterCount;
	SpawnedMonsters = 0;
	CurrentStageState = EPGStageState::InProgress;
	
	// 델리게이트 호출
	OnStageStarted.Broadcast(CurrentStageId);
	OnMonsterCountChanged.Broadcast(RemainingMonsters);
	
	UE_LOG(LogTemp, Log, TEXT("PGStageManager: 스테이지 %d 시작 - 몬스터 %d마리"), 
		   CurrentStageId, CurrentStageDataCache.TotalMonsterCount);
	
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
		while (SpawnedMonsters < CurrentStageDataCache.TotalMonsterCount)
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

void APGStageManager::SpawnEnemyBatch()
{
	if (CurrentStageState != EPGStageState::InProgress)
	{
		return;
	}
	
	// 스폰할 몬스터 수 계산
	int32 MonstersToSpawn = FMath::Min(CurrentStageDataCache.SpawnBatchSize, 
									  CurrentStageDataCache.TotalMonsterCount - SpawnedMonsters);
	
	// 배치 스폰
	for (int32 i = 0; i < MonstersToSpawn; i++)
	{
		if (CurrentStageDataCache.SpawnTargets.Num() > 0)
		{
			// 랜덤 적 타입 선택
			int32 RandomIndex = FMath::RandRange(0, CurrentStageDataCache.SpawnTargets.Num() - 1);
			int32 EnemyId = CurrentStageDataCache.SpawnTargets[RandomIndex];
			
			APGCharacterEnemy* SpawnedEnemy = SpawnSingleEnemy(EnemyId);
			if (SpawnedEnemy)
			{
				SpawnedMonsters++;
				SpawnedEnemies.Add(SpawnedEnemy);
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("PGStageManager: %d마리 몬스터 스폰됨 (총 %d/%d)"), 
		   MonstersToSpawn, SpawnedMonsters, CurrentStageDataCache.TotalMonsterCount);
	
	// 모든 몬스터 스폰 완료 시 타이머 정지
	if (SpawnedMonsters >= CurrentStageDataCache.TotalMonsterCount)
	{
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
		UE_LOG(LogTemp, Log, TEXT("PGStageManager: 모든 몬스터 스폰 완료"));
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

	
	// 스폰 위치 계산
	FVector SpawnLocation = GetRandomSpawnLocation();
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

void APGStageManager::OnEnemyKilled(APGCharacterEnemy* KilledEnemy)
{

}

void APGStageManager::ShowRewardSelection()
{
	// TODO: 보상 선택 UI 표시
	
	// 테스트용: 3초 후 자동으로 보상 선택 완료
	FTimerHandle AutoRewardTimer;
	GetWorld()->GetTimerManager().SetTimer(AutoRewardTimer, 
		this, &APGStageManager::OnRewardSelected, 3.0f, false);
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

FPGStageDataRow APGStageManager::GetCurrentStageDataCopy() const
{
	return CurrentStageDataCache;
}