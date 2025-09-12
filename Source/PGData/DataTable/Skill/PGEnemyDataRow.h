#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGEnemyDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGEnemyDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 EnemyID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName EnemyName;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int32> SkillIdList;
};
