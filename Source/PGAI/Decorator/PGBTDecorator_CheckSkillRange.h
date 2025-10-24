// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "PGBTDecorator_CheckSkillRange.generated.h"

enum class EPGSkillType : uint8;

/**
 * 
 */
UCLASS()
class PGAI_API UPGBTDecorator_CheckSkillRange : public UBTDecorator
{
	GENERATED_BODY()

protected:
	/** Blackboard - 선택된 스킬 ID 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector SelectedSkillIDKey;
	
	/** Blackboard - 타겟 액터 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector TargetActorKey;

	/** Blackboard - Self 액터 키 */
	UPROPERTY(EditAnywhere, Category = "PG|AI")
	FBlackboardKeySelector SelfActorKey;
	
public:
	UPGBTDecorator_CheckSkillRange();

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;
	
private:
	/** Enemy가 해당 스킬 타입을 보유하고 있는지 체크 */
	bool HasSkillType(int32 EnemyTID, EPGSkillType SkillType) const;
	
	/** 스킬 데이터에서 최대 거리 가져오기 */
	float GetMaxDistanceFromSkillData(int32 SkillID) const;
};
