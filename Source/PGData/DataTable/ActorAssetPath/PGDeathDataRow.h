#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGDeathDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGDeathDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 ObjectTID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FSoftObjectPath> DeathMontagePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSoftObjectPtr<class UNiagaraSystem> DissolveVFXPath;
};
