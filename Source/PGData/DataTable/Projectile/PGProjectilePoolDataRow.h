#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGShared/Shared/Enum/PGProjectileEnumType.h"
#include "PGProjectilePoolDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGProjectilePoolDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(SearchKey = "True"))
	EPGProjectileType ProjectileType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSoftClassPath ProjectileClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PoolSize = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxPoolSize = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DefaultLifeTime = 5.0f;
};
