#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "UObject/SoftObjectPath.h"
#include "PGRewardItemDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGRewardItemDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 ItemId;
};
