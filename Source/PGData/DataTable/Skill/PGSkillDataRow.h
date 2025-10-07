#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGSkillDataRow.generated.h"

USTRUCT(BlueprintType)
struct PGDATA_API FPGSkillDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(SearchKey = "True"))
	int32 SkillID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Desc;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EPGSkillType SkillType;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSoftObjectPath MontagePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSoftObjectPath SkillIconPath;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int32> ChainSkillIdList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SkillCoolTime;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 InitialPriority = 1;

	/** 스킬 사용 가능 범위 (0이면 범위 제한 없음) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float SkillRange = 0.f;
	
};
