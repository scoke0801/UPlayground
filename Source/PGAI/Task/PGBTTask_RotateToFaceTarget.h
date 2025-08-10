// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "PGBTTask_RotateToFaceTarget.generated.h"

/*
 * 비헤이비어 트리 태스크 실행 중, 사용할 메모리 구조체
 * 회전을 수행하는 폰과 타겟 액터에 대한 정보를 저장
 */
struct FRotateToFaceTargetTaskMemory
{
	TWeakObjectPtr<APawn> OwningPawn;
	TWeakObjectPtr<AActor> TargetActor;

public:
	bool IsValid() const;
	void Reset();
};
/**
 * Behavior Tree Task 노드로, AI 폰이 지정된 타겟을 향해 부드럽게 회전하도록 하는 기능을 제공
 * 각도 정밀도에 도달할 때까지 설정된 속도로 점진적으로 회전을 수행
 */
UCLASS()
class PGAI_API UPGBTTask_RotateToFaceTarget : public UBTTaskNode
{
	GENERATED_BODY()

private:
	UPROPERTY(EditAnywhere, Category = "PG|Face Target")
	float AnglePrecision;

	UPROPERTY(EditAnywhere, Category = "PG|Face Target")
	float RotationInterpSpeed;

	UPROPERTY(EditAnywhere, Category = "PG|Face Target")
	FBlackboardKeySelector InTargetToFaceKey;

private:
	UPGBTTask_RotateToFaceTarget();

private:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	virtual uint16 GetInstanceMemorySize() const override;

	virtual FString GetStaticDescription() const override;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
	bool HasReachedAnglePrecision(APawn* QueryPawn, AActor* TargetActor);
};
