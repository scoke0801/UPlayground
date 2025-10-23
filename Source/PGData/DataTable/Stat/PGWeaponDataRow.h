#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "UObject/SoftObjectPath.h"
#include "PGWeaponDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGWeaponDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 WeaponID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<EPGStatType, int32> Stats;
	

};
