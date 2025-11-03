#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "UObject/SoftObjectPath.h"
#include "PGRewardSkillDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGRewardSkillDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 SkillId;
};
