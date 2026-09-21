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
	int32 SkillID = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Desc;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EPGSkillType SkillType = {};
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSoftObjectPath MontagePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSoftObjectPath SkillIconPath;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int32> ChainSkillIdList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SkillCoolTime = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 InitialPriority = 1;

	/** 스킬 사용 가능 범위 (0이면 범위 제한 없음) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float SkillRange = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cancel", meta=(ClampMin="0", ClampMax="1"))
    float AttackCancelRemainingFraction = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cancel", meta=(ClampMin="0", ClampMax="1"))
    float DodgeCancelRemainingFraction = 0.5f;

    // A positive duration opts this skill into the elite slam. One radius owns decal and hit test.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="0"))
    float TelegraphDuration = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="0"))
    float RecoveryDuration = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="0", ClampMax="2"))
    float RecoveryDamageBonus = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="1"))
    float TelegraphRadius = 280.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph")
    TSoftObjectPtr<class UMaterialInterface> TelegraphMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph")
    TSoftObjectPtr<class UNiagaraSystem> SlamVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph")
    TSoftObjectPtr<class UAnimMontage> ElitePresentationMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telegraph", meta=(ClampMin="0", ClampMax="1"))
    float WindupMontageFraction = .25f;
	
};
