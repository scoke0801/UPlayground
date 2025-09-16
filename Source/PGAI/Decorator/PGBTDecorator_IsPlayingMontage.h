// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "PGBTDecorator_IsPlayingMontage.generated.h"

/**
 * 
 */
UCLASS()
class PGAI_API UPGBTDecorator_IsPlayingMontage : public UBTDecorator
{
	GENERATED_BODY()

protected:
	// 몽타쥬 재생 중일 때 조건 반전 여부
	UPROPERTY(EditAnywhere, Category = "PG|Montage")
	bool bInvertCondition = true;

public:
	UPGBTDecorator_IsPlayingMontage();
	
protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;
};
