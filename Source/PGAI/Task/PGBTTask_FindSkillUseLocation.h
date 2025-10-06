// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "PGBTTask_FindSkillUseLocation.generated.h"

class UEnvQuery;

/**
 * BT Task: 스킬 사용 가능한 위치 찾기 (EQS)
 * - SelectedSkillID로 스킬 데이터 조회
 * - EQS를 통해 스킬 사용 가능한 최적 위치 탐색
 * - 결과를 Blackboard Vector 키에 저장
 * - 실제 이동은 별도의 MoveTo Task 사용
 */
UCLASS()
class PGAI_API UPGBTTask_FindSkillUseLocation : public UBTTaskNode
{
	GENERATED_BODY()

protected:
	/** EQS - 스킬 사용 위치 탐색 쿼리 */
	UPROPERTY(EditAnywhere, Category = "PG|AI|EQS")
	UEnvQuery* FindSkillLocationQuery;

	/** Blackboard - 타겟 액터 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector TargetActorKey;

	/** Blackboard - 선택된 스킬 ID 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector SelectedSkillIDKey;

	/** Blackboard - EQS 결과 위치를 저장할 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector SkillLocationKey;

private:
	/** BehaviorTreeComponent 약한 참조 */
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;

public:
	UPGBTTask_FindSkillUseLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private:
	/** 스킬 데이터에서 최대 거리 가져오기 */
	float GetMaxDistanceFromSkillData(int32 SkillID) const;

	/** EQS 쿼리 완료 콜백 */
	void OnEQSQueryFinished(TSharedPtr<FEnvQueryResult> Result);
};
