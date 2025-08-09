// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "PGBTService_OrientToTargetActor.generated.h"

/**
 * AI 폰이 지속적으로 대상 액터를 향하도록 회전시키는 Behavior Tree Service
 * 부드러운 회전 보간을 사용하여 시간에 걸쳐 대상을 향해 회전함
 */
UCLASS()
class PGAI_API UPGBTService_OrientToTargetActor : public UBTService
{
	GENERATED_BODY()

private:
	UPROPERTY(EditAnywhere, Category="PG|Target")
	FBlackboardKeySelector InTargetActorKey;

	UPROPERTY(EditAnywhere, Category="PG|Rotate")
	float RotationInterpSpeed;

private:
	UPGBTService_OrientToTargetActor();

private:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	virtual FString GetStaticDescription() const override;

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
