#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
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
	EPGRewardType RewardType;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RewardItemId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int RewardSkillId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EPGStatType RewardStatType;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int RewardStatValue;
};
