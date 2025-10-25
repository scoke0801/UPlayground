#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGRewardDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGRewardDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 Id;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RewardItemId;

};
