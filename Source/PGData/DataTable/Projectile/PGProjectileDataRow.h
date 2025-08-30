#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PGShared/Shared/Enum/PGProjectileEnumType.h"
#include "UObject/SoftObjectPath.h"
#include "PGProjectileDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGProjectileDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 ProjectileId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPGProjectileType ProjectileType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSoftClassPath ProjectileClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DefaultSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DefaultDamage = 10.0f;
};
