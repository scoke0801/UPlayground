// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGBTTask_ExecuteSkill.generated.h"

/**
 * BT Task: 선택된 스킬을 실행하는 Task
 * Blackboard에 저장된 스킬 ID를 읽어서 GAS를 통해 어빌리티 실행
 */
UCLASS()
class PGAI_API UPGBTTask_ExecuteSkill : public UBTTaskNode
{
	GENERATED_BODY()

protected:
	/** Blackboard - 타겟 키*/
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector TargetActorKey;

	/** Blackboard - 스킬 ID 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector SelectedSkillIDKey;

	/** Blackboard - 소환 카운트 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector SummonCountKey;

	/** Blackboard - StrafeState */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector StrafeKey;

	/** 힐 아군 탐색 반경 */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Heal")
	float AllySearchRadius = 1500.f;

private:
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;

	/** 현재 실행 중인 스킬 타입 (카운트 증가용) */
	EPGSkillType CachedSkillType;
	
public:
	UPGBTTask_ExecuteSkill();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	/** Task 완료 처리 */
	void FinishTask(UBehaviorTreeComponent* OwnerComp, EBTNodeResult::Type Result);

	/** 힐 스킬을 위한 최적의 아군 타겟 선택 */
	AActor* SelectBestHealTarget(class APGCharacterEnemy* Self, UBlackboardComponent* BlackboardComp) const;

	/** 스킬 타입을 GameplayTag로 변환 */
	FGameplayTag GetAbilityTagFromSkillType(EPGSkillType SkillType) const;
	
	bool CheckExecuteStrafe(EPGSkillType SkillType);
};
