// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "PGBTTask_MoveToSkillRange.generated.h"

struct FPathFollowingResult;

/**
 * BT Task: 스킬 사용 범위 내로 이동
 * - SelectedSkillID로 스킬 데이터 조회
 * - 스킬 범위(SkillRange)를 확인하여 타겟에게 접근
 * - 이미 범위 내부면 즉시 성공
 * - 이동 중 주기적으로 거리 체크하여 범위 진입 시 즉시 중단
 */
UCLASS()
class PGAI_API UPGBTTask_MoveToSkillRange : public UBTTaskNode
{
	GENERATED_BODY()

protected:
	/** Blackboard - 타겟 액터 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector TargetActorKey;

	/** Blackboard - 선택된 스킬 ID 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector SelectedSkillIDKey;

	/** 스킬 범위에서 제외할 버퍼 거리 (안전 여유) */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Movement")
	float RangeBuffer = 50.0f;

	/** 이미 범위 내부일 때 즉시 성공 처리할 허용 거리 */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Movement")
	float AcceptableRadiusMargin = 100.0f;

	/** 스킬 범위가 0일 때 사용할 기본 거리 */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Movement")
	float DefaultSkillRange = 300.0f;

	/** 거리 체크 간격 (초) */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Performance")
	float DistanceCheckInterval = 0.2f;

private:
	/** 이동 요청 ID (중단 처리용) */
	FAIRequestID MoveRequestID;

	/** 거리 체크용 타이머 */
	FTimerHandle DistanceCheckTimerHandle;

	/** BehaviorTreeComponent 약한 참조 */
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;

	/** 캐시된 스킬 범위 */
	float CachedSkillRange = 0.0f;

public:
	UPGBTTask_MoveToSkillRange();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	/** PathFollowing 완료 콜백 */
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private:
	/** 스킬 데이터에서 스킬 범위 가져오기 */
	float GetSkillRangeFromData(int32 SkillID) const;

	/** 이동 완료 콜백 */
	void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

	/** 타이머 기반 거리 체크 */
	void CheckDistanceToTarget();

	/** 타이머 정리 */
	void ClearDistanceCheckTimer();
};
