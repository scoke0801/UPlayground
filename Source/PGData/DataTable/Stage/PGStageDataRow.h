#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PGShared/Shared/Enum/PGRewardTypes.h"
#include "UObject/SoftObjectPath.h"
#include "PGStageDataRow.generated.h"

// 몬스터 타입별 스폰 정보 구조체
USTRUCT(BlueprintType)
struct PGDATA_API FPGMonsterSpawnInfo
{
	GENERATED_BODY()

public:
	// 몬스터 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Info")
	int32 MonsterId = 0;

	// 이 몬스터 타입의 스폰 수량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Info", meta = (ClampMin = "0"))
	int32 SpawnCount = 1;

	// 스폰 우선순위 (높을수록 먼저 스폰, 0이면 랜덤)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Info", meta = (ClampMin = "0"))
	int32 SpawnPriority = 0;

	// 이 몬스터가 스폰되기 시작하는 시점 (웨이브 시작 후 초 단위)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Info", meta = (ClampMin = "0.0"))
	float SpawnDelayTime = 0.0f;

public:
	FPGMonsterSpawnInfo()
		: MonsterId(0), SpawnCount(1), SpawnPriority(0), SpawnDelayTime(0.0f)
	{
	}

	FPGMonsterSpawnInfo(int32 InMonsterId, int32 InSpawnCount, int32 InSpawnPriority = 0, float InSpawnDelayTime = 0.0f)
		: MonsterId(InMonsterId), SpawnCount(InSpawnCount), SpawnPriority(InSpawnPriority), SpawnDelayTime(InSpawnDelayTime)
	{
	}
};


USTRUCT(BlueprintType)
struct PGDATA_API FPGStageWave
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
    TArray<FPGMonsterSpawnInfo> MonsterSpawnInfos;

    // 이전 웨이브 전멸 후 대기 시간. 첫 웨이브에는 스테이지 시작부터 적용한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0"))
    float StartDelay = 2.f;
};

USTRUCT(BlueprintType)
struct PGDATA_API FPGStageReward
{
	GENERATED_BODY()

public:
	// 보상 타입
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PG|Reward")
	EPGRewardType RewardType = EPGRewardType::None;
    
	// 보상 ID
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PG|Reward")
	int32 RewardId = 0;
    
	// 가중치 (높을수록 나올 확률 높음)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PG|Reward", meta=(ClampMin = "0.1"))
	float Weight = 1.0f;
    
	FPGStageReward()
	{
		RewardType = EPGRewardType::None;
		RewardId = 0;
		Weight = 1.0f;
	}
};

USTRUCT(BlueprintType)
struct PGDATA_API FPGStageDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 Id = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info")
	FString StageName = TEXT("Stage");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info")
	bool bIsBossStage = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info", meta = (ClampMin = "0.0"))
	float NextStageDelay = 3.0f; // Legacy serialized field; BuildDuration replaces it.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info", meta = (ClampMin = "0"))
    float BuildDuration = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stage Info")
    bool bManualReady = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rewards", meta=(ClampMin="1", ClampMax="3"))
    int32 RewardSelections = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
    TArray<FPGStageWave> Waves;
	
	// 이전 데이터 호환: Waves가 비어 있을 때 단일 웨이브로 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	TArray<FPGMonsterSpawnInfo> MonsterSpawnInfos;

	// 스폰 간격 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn", meta = (ClampMin = "0.1"))
	float SpawnInterval = 2.0f;

	// 한 번에 스폰할 최대 몬스터 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn", meta = (ClampMin = "1"))
	int32 SpawnBatchSize = 3;

	// 스폰 반경
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn", meta = (ClampMin = "100.0"))
	float SpawnRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn", meta=(ClampMin="1"))
    int32 MaxSpawnRetries = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
    bool bCountSummonedEnemies = true;

    // 보상 풀
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	TArray<FPGStageReward> RewardPool;
};
