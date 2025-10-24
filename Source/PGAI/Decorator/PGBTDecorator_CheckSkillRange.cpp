// Fill out your copyright notice in the Description page of Project Settings.


#include "PGBTDecorator_CheckSkillRange.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

UPGBTDecorator_CheckSkillRange::UPGBTDecorator_CheckSkillRange()
{
	NodeName = "Check Skill Range";
	
	// 조건이 변경될 때 노드를 다시 평가
	bNotifyBecomeRelevant = true;
	bNotifyTick = true;
	
	TargetActorKey.SelectedKeyName = "TargetActor";
	SelfActorKey.SelectedKeyName = "SelfActorKey";
	SelectedSkillIDKey.SelectedKeyName = FName("SelectedSkillID");
}

bool UPGBTDecorator_CheckSkillRange::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (nullptr == Blackboard)
	{
		return false;
	}

	// 타겟 액터 확인
	AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!TargetActor)
	{
		return false;
	}

	// Self 액터 확인
	AActor* SelfActor = Cast<AActor>(Blackboard->GetValueAsObject(SelfActorKey.SelectedKeyName));
	if (!SelfActor)
	{
		return false;
	}
	
	// 선택된 스킬 ID 가져오기
	const int32 SkillID = Blackboard->GetValueAsInt(SelectedSkillIDKey.SelectedKeyName);
	if (SkillID <= 0)
	{
		return false;
	}
	
	float SkillUsableDist = GetMaxDistanceFromSkillData(SkillID);
	if (FMath::IsNearlyZero(SkillUsableDist))
	{
		// 스킬 제한 거리가 없는 경우 항상 true
		return true;
	}
	float DistToTarget = FVector::Dist2D(TargetActor->GetActorLocation(),SelfActor->GetActorLocation());
	return DistToTarget <= SkillUsableDist;
}

FString UPGBTDecorator_CheckSkillRange::GetStaticDescription() const
{
	return Super::GetStaticDescription();
}

bool UPGBTDecorator_CheckSkillRange::HasSkillType(int32 EnemyTID, EPGSkillType SkillType) const
{
	// 캐시 생성
	UPGDataTableManager* DTManager = UPGDataTableManager::Get();
	if (!DTManager) return false;
	
	const FPGEnemyDataRow* EnemyData = DTManager->GetEnemyDataRowByKey(EnemyTID);
	if (!EnemyData) return false;
	
	TSet<EPGSkillType> SkillTypes;
	for (int32 SkillID : EnemyData->SkillIdList)
	{
		const FPGSkillDataRow* SkillData = DTManager->GetSkillDataRowByKey(SkillID);
		if (SkillData)
		{
			SkillTypes.Add(SkillData->SkillType);
		}
	}
	
	return SkillTypes.Contains(SkillType);
}

float UPGBTDecorator_CheckSkillRange::GetMaxDistanceFromSkillData(int32 SkillID) const
{
	if (UPGDataTableManager* DataTableManager = UPGDataTableManager::Get())
	{
		if (const FPGSkillDataRow* SkillData = DataTableManager->GetRowData<FPGSkillDataRow>(SkillID))
		{
			if (SkillData->SkillRange > 0.0f)
			{
				return SkillData->SkillRange;
			}
		}
	}

	return 0.0f;
}
