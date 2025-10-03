// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "PGBTTask_ExecuteSkill.generated.h"

/**
 * BT Task: 선택된 스킬을 실행하는 Task
 * Blackboard에 저장된 스킬 ID를 읽어서 몽타주를 재생
 */
UCLASS()
class PGAI_API UPGBTTask_ExecuteSkill : public UBTTaskNode
{
	GENERATED_BODY()

protected:
	/** Blackboard에서 읽을 타겟 키*/
	UPROPERTY(EditAnywhere, Category = "AI")
	FBlackboardKeySelector TargetActorKey;

	/** Blackboard에서 읽을 스킬 ID 키 */
	UPROPERTY(EditAnywhere, Category = "AI")
	FBlackboardKeySelector SelectedSkillIDKey;

private:
	/** 현재 실행 중인 BT 컴포넌트 */
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
	
public:
	UPGBTTask_ExecuteSkill();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	/** 몽타주 재생 완료 콜백 */
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Task 완료 처리 */
	void FinishTask(UBehaviorTreeComponent* OwnerComp, EBTNodeResult::Type Result);
};
