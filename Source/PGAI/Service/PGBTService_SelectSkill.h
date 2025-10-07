// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGBTService_SelectSkill.generated.h"

/**
 * BT Service: 사용 가능한 스킬 중에서 하나를 선택
 * Enemy 데이터에 설정된 스킬 리스트에서 조건에 맞는 스킬을 선택
 */
UCLASS()
class PGAI_API UPGBTService_SelectSkill : public UBTService
{
	GENERATED_BODY()

protected:
	/** Blackboard - 선택된 스킬 ID 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector SelectedSkillIDKey;

	/** Blackboard - 대상 액터 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector TargetActorKey;

	/** 스킬 선택 가중치 설정 */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Skill Selection")
	bool bUseWeightedSelection = true;

	/** Melee 스킬 최대 거리 */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Skill Selection")
	float MeleeSkillMaxRange = 300.f;

	/** Range 스킬 최소 거리 */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Skill Selection")
	float RangeSkillMinRange = 500.f;

	/** HP가 이 비율 이하일 때 Heal 스킬 우선 */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Skill Selection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealSkillHPThreshold = 0.3f;

	/** 아군 탐색 반경 (힐 스킬용) */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Skill Selection|Heal")
	float AllySearchRadius = 1500.f;

	/** 아군이 이 HP 비율 이하일 때 힐 필요 판단 */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Skill Selection|Heal", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AllyHealThreshold = 0.6f;

	/** 전투 당 최대 소환 횟수 */
	UPROPERTY(EditAnywhere, Category = "PG|AI|Skill Selection|Summon")
	int32 MaxSummonCountPerBattle = 2;

	/** 소환 횟수를 저장할 Blackboard 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector SummonCountKey;
	
public:
	UPGBTService_SelectSkill();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
	/** 스킬 선택 로직 */
	int32 SelectBestSkill(const TArray<int32>& SkillIDList, float DistanceToTarget, float CurrentHPRatio, class APGCharacterEnemy* Enemy, class UBlackboardComponent* BlackboardComp) const;

	/** 스킬 타입별 우선순위 계산 */
	float CalculateSkillPriority(EPGSkillType SkillType, float DistanceToTarget, float CurrentHPRatio, class APGCharacterEnemy* Enemy, class UBlackboardComponent* BlackboardComp, const TSet<EPGSkillType>& AvailableSkillTypes) const;

	/** 회복이 필요한 아군이 있는지 체크 */
	bool CheckNeedHealAlly(class APGCharacterEnemy* Enemy) const;

	/** Enemy가 해당 스킬 타입을 보유하고 있는지 체크 */
	bool HasSkillType(int32 EnemyTID, EPGSkillType SkillType) const;

	/** 스킬 타입 캐싱 (성능 최적화) */
	mutable TMap<int32, TSet<EPGSkillType>> CachedEnemySkillTypes;
};
