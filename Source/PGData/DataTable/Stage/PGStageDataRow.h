#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
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

	// 이 몬스터가 스폰되기 시작하는 시점 (스테이지 시작 후 초 단위)
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
struct PGDATA_API FPGStageDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info")
	FString StageName = TEXT("Stage");

	// 개선된 몬스터 스폰 정보 (몬스터별 수량 지정 가능)
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	TArray<int32> PossibleRewards;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info", meta = (ClampMin = "0.0"))
	float NextStageDelay = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info")
	bool bIsBossStage = false;
};
