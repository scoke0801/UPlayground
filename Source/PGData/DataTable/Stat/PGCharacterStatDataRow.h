#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGCharacterStatDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGCharacterStatDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 CharacterID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	TMap<EPGStatType, int32> Stats;
	

};
