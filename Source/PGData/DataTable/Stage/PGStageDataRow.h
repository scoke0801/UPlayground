#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGStageDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGStageDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info")
	FString StageName = TEXT("Stage");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	TArray<int32> SpawnTargets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	int32 TotalMonsterCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	float SpawnInterval = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	int32 SpawnBatchSize = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	float SpawnRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	TArray<int32> PossibleRewards;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info")
	float NextStageDelay = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info")
	bool bIsBossStage = false;
};
