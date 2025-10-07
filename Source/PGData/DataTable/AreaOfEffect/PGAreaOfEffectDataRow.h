#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PGShared/Shared/Enum/PGEnumTypes.h"
#include "UObject/SoftObjectPath.h"
#include "PGAreaOfEffectDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGAreaOfEffectDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 EffectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPGEffectType EffectType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSoftClassPath EffectPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LifeTime = 5.0f;
};
