#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PGShared/Shared/Enum/PGEnumTypes.h"
#include "UObject/SoftObjectPath.h"
#include "NiagaraSystem.h"
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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, 
		meta=(EditCondition="EffectType==EPGEffectType::Niagara", EditConditionHides))
	TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, 
		meta=(EditCondition="EffectType==EPGEffectType::Legacy", EditConditionHides))
	TSoftObjectPtr<UParticleSystem> ParticleSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LifeTime = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TickInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector BoxExtent = FVector(100.0f, 100.0f, 100.0f);
};
