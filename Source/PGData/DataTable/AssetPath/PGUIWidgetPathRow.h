#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGUIWidgetPathRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGUIWidgetPathRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	FString Key;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Desc;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSoftObjectPath AssetPath;
};
