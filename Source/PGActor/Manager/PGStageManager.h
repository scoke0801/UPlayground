#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "PGData/DataTable/Stage/PGStageDataRow.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"
#include "PGStageManager.generated.h"

UENUM(BlueprintType)
enum class EPGStageState : uint8
{
    None        UMETA(DisplayName = "None"),
    InProgress  UMETA(DisplayName = "In Progress"),
    RewardPhase UMETA(DisplayName = "Reward Phase"),
    Completed   UMETA(DisplayName = "Completed")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStageStarted, int32, StageNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStageCompleted, int32, StageNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMonsterCountChanged, int32, RemainingCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllMonstersKilled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemySpawned, class APGCharacterEnemy*, SpawnedEnemy);

UCLASS(BlueprintType, Blueprintable)
class PGACTOR_API APGStageManager : public AActor
{
	GENERATED_BODY()
	
private:
	// 현재 스테이지 ID
	UPROPERTY(VisibleAnywhere, Category = "Stage Info", meta = (AllowPrivateAccess = "true"))
	int32 CurrentStageId = 1;
	
	// 현재 스테이지 상태
	UPROPERTY(VisibleAnywhere, Category = "Stage Info", meta = (AllowPrivateAccess = "true"))
	EPGStageState CurrentStageState = EPGStageState::None;
	
	// 남은 몬스터 수
	UPROPERTY(VisibleAnywhere, Category = "Stage Info", meta = (AllowPrivateAccess = "true"))
	int32 RemainingMonsters = 0;
	
	// 스폰된 몬스터 수
	UPROPERTY(VisibleAnywhere, Category = "Stage Info", meta = (AllowPrivateAccess = "true"))
	int32 SpawnedMonsters = 0;
	
	// 현재 스테이지 데이터 (값으로 저장하여 안전성 확보)
	FPGStageDataRow CurrentStageDataCache;
	
	// 스폰 타이머
	UPROPERTY()
	FTimerHandle SpawnTimer;
		
	// 다음 스테이지 시작 타이머
	UPROPERTY()
	FTimerHandle NextStageTimer;

	// 스폰된 적들 추적dddd
	UPROPERTY()
	TArray<APGCharacterEnemy*> SpawnedEnemies;

private:
	FDelegateHandle OnActorDiedHandle;

public:    
	APGStageManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnStageStarted OnStageStarted;
	
	UPROPERTY(BlueprintAssignable)
	FOnStageCompleted OnStageCompleted;
	
	UPROPERTY(BlueprintAssignable)
	FOnMonsterCountChanged OnMonsterCountChanged;
	
	UPROPERTY(BlueprintAssignable)
	FOnAllMonstersKilled OnAllMonstersKilled;

	UPROPERTY(BlueprintAssignable)
	FOnEnemySpawned OnEnemySpawned;

public:
	// 스테이지 시작
	UFUNCTION(BlueprintCallable, Category = "Stage Management")
	void StartStage(int32 StageId = -1);
	
	// 몬스터 처치 알림
	UFUNCTION(BlueprintCallable, Category = "Stage Management")
	void OnEnemyKilled(APGCharacterEnemy* KilledEnemy);
	
	// 보상 선택 완료
	UFUNCTION(BlueprintCallable, Category = "Stage Management")
	void OnRewardSelected();
	
	// 다음 스테이지로 진행
	UFUNCTION(BlueprintCallable, Category = "Stage Management")
	void GoToNextStage();

	// 모든 적 강제 제거
	UFUNCTION(BlueprintCallable, Category = "Stage Management")
	void ClearAllEnemies();

private:
	// 스테이지 데이터 로드
	bool LoadStageData(int32 StageId);
	
	// 몬스터 배치 스폰
	UFUNCTION()
	void SpawnEnemyBatch();
	
	// 단일 몬스터 스폰
	APGCharacterEnemy* SpawnSingleEnemy(int32 EnemyId);
	
	// 스폰 위치 계산
	FVector GetRandomSpawnLocation() const;
	
	// 지형을 고려한 안전한 스폰 위치 찾기
	FVector GetSafeSpawnLocation() const;
	
	// 스폰 위치 유효성 검증
	bool IsValidSpawnLocation(const FVector& Location) const;
	
	// 경사면 각도 검증
	bool IsValidSlope(const FVector& Location) const;
	
	// 보상 UI 표시
	void ShowRewardSelection();
	
	// 다음 스테이지 시작 (딜레이 후)
	UFUNCTION()
	void StartNextStageAfterDelay();

private:
	// 액터 사망 시 메시지 수신
	void OnActorDied(const IPGEventData* IpgEventData);
	
public:
	// Getter 함수들
	UFUNCTION(BlueprintCallable, Category = "Stage Info")
	int32 GetCurrentStageId() const { return CurrentStageId; }
	
	UFUNCTION(BlueprintCallable, Category = "Stage Info")
	EPGStageState GetCurrentStageState() const { return CurrentStageState; }
	
	UFUNCTION(BlueprintCallable, Category = "Stage Info")
	int32 GetRemainingMonsters() const { return RemainingMonsters; }
	
	UFUNCTION(BlueprintCallable, Category = "Stage Info")
	int32 GetSpawnedMonsters() const { return SpawnedMonsters; }

	UFUNCTION(BlueprintCallable, Category = "Stage Info")
	FPGStageDataRow GetCurrentStageDataCopy() const;

	UFUNCTION(BlueprintCallable, Category = "Stage Info")
	bool IsStageInProgress() const { return CurrentStageState == EPGStageState::InProgress; }
};