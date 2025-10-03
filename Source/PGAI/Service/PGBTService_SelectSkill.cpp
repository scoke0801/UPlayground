// Fill out your copyright notice in the Description page of Project Settings.

#include "PGBTService_SelectSkill.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Stat/PGEnemyStatComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGData/DataTable/Skill/PGEnemyDataRow.h"

UPGBTService_SelectSkill::UPGBTService_SelectSkill()
{
	NodeName = TEXT("Select Skill");
	Interval = 1.0f;
	RandomDeviation = 0.2f;
	
	SelectedSkillIDKey.SelectedKeyName = FName("SelectedSkillID");
	TargetActorKey.SelectedKeyName = FName("TargetActor");
}

void UPGBTService_SelectSkill::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return;

	APGCharacterEnemy* Enemy = Cast<APGCharacterEnemy>(AIController->GetPawn());
	if (!Enemy) return;

	UPGDataTableManager* DTManager = UPGDataTableManager::Get();
	if (!DTManager) return;
	
	const FPGEnemyDataRow* EnemyData = DTManager->GetEnemyDataRowByKey(Enemy->GetCharacterTID());
	if (!EnemyData || EnemyData->SkillIdList.Num() == 0)
	{
		return;
	}
	
	// 타겟과의 거리 계산
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName));
	
	float DistanceToTarget = -1.f;
	if (TargetActor)
	{
		DistanceToTarget = FVector::Dist(Enemy->GetActorLocation(), TargetActor->GetActorLocation());
	}
	
	// 현재 HP 비율 계산
	const UPGEnemyStatComponent* StatComp = Enemy->GetEnemyStatComponent();
	const float CurrentHPRatio = StatComp ? (StatComp->CurrentHP / StatComp->MaxHP) : 1.f;
	
	// 스킬 선택
	const int32 SelectedSkillID = SelectBestSkill(EnemyData->SkillIdList, DistanceToTarget, CurrentHPRatio);
	
	if (SelectedSkillID > 0)
	{
		BlackboardComp->SetValueAsInt(SelectedSkillIDKey.SelectedKeyName, SelectedSkillID);
	}
}

int32 UPGBTService_SelectSkill::SelectBestSkill(const TArray<int32>& SkillIDList, float DistanceToTarget, float CurrentHPRatio) const
{
	UPGDataTableManager* DTManager = UPGDataTableManager::Get();
	if (!DTManager || SkillIDList.Num() == 0)
	{
		return -1;
	}

	// 가중치 기반 선택
	TArray<int32> ValidSkills;
	TArray<float> Weights;

	for (const int32 SkillID : SkillIDList)
	{
		const FPGSkillDataRow* SkillData = DTManager->GetSkillDataRowByKey(SkillID);
		if (!SkillData) continue;
	
		const float Priority = CalculateSkillPriority(SkillData->SkillType, DistanceToTarget, CurrentHPRatio);
		if (Priority > 0.f)
		{
			ValidSkills.Add(SkillID);
			Weights.Add(Priority);
		}
	}
	
	if (ValidSkills.Num() == 0)
	{
		// 조건에 맞는 스킬이 없으면 첫 번째 스킬 반환
		return SkillIDList[0];
	}
	
	if (!bUseWeightedSelection || Weights.Num() == 0)
	{
		// 랜덤 선택
		return ValidSkills[FMath::RandRange(0, ValidSkills.Num() - 1)];
	}
	
	// 가중치 기반 랜덤 선택
	float TotalWeight = 0.f;
	for (float Weight : Weights)
	{
		TotalWeight += Weight;
	}
	
	const float RandomValue = FMath::FRandRange(0.f, TotalWeight);
	float AccumulatedWeight = 0.f;
	
	for (int32 i = 0; i < ValidSkills.Num(); ++i)
	{
		AccumulatedWeight += Weights[i];
		if (RandomValue <= AccumulatedWeight)
		{
			return ValidSkills[i];
		}
	}

	return ValidSkills[0];
}

float UPGBTService_SelectSkill::CalculateSkillPriority(EPGSkillType SkillType, float DistanceToTarget, float CurrentHPRatio) const
{
	switch (SkillType)
	{
	case EPGSkillType::Melee:
		{
			// 근접: 가까울수록 우선순위 높음
			if (DistanceToTarget < 0.f) return 1.f;
			return DistanceToTarget <= MeleeSkillMaxRange ? 3.f : 0.5f;
		}
		
	case EPGSkillType::Range:
		{
			// 원거리: 멀수록 우선순위 높음
			if (DistanceToTarget < 0.f) return 1.f;
			return DistanceToTarget >= RangeSkillMinRange ? 3.f : 1.f;
		}
		
	case EPGSkillType::Heal:
		{
			// 힐: HP가 낮을수록 우선순위 높음
			if (CurrentHPRatio <= HealSkillHPThreshold)
			{
				return 5.f; // 매우 높은 우선순위
			}
			return 0.1f; // HP가 충분하면 낮은 우선순위
		}
		
	case EPGSkillType::SummonEnemy:
		{
			// 소환: 기본 우선순위
			return 1.5f;
		}
		
	default:
		return 1.f;
	}
}
