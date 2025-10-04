// Fill out your copyright notice in the Description page of Project Settings.

#include "PGBTService_SelectSkill.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
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
	SummonCountKey.SelectedKeyName = FName("SummonCount");
}

void UPGBTService_SelectSkill::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return;
	}
	
	APGCharacterEnemy* Enemy = Cast<APGCharacterEnemy>(AIController->GetPawn());
	if (!Enemy)
	{
		return;
	}
	
	UPGDataTableManager* DTManager = UPGDataTableManager::Get();
	if (!DTManager)
	{
		return;
	}
	
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
	const int32 SelectedSkillID = SelectBestSkill(EnemyData->SkillIdList, DistanceToTarget, CurrentHPRatio, Enemy, BlackboardComp);
	
	if (SelectedSkillID > 0)
	{
		BlackboardComp->SetValueAsInt(SelectedSkillIDKey.SelectedKeyName, SelectedSkillID);
	}
}

int32 UPGBTService_SelectSkill::SelectBestSkill(const TArray<int32>& SkillIDList, float DistanceToTarget, float CurrentHPRatio, APGCharacterEnemy* Enemy, UBlackboardComponent* BlackboardComp) const
{
	UPGDataTableManager* DTManager = UPGDataTableManager::Get();
	if (!DTManager || SkillIDList.Num() == 0)
	{
		return -1;
	}

	// 가중치 기반 선택
	TArray<int32> ValidSkills;
	TArray<float> Weights;

	// 스킬 타입별 존재 여부 사전 체크 (최적화)
	TSet<EPGSkillType> AvailableSkillTypes;
	for (const int32 SkillID : SkillIDList)
	{
		const FPGSkillDataRow* SkillData = DTManager->GetSkillDataRowByKey(SkillID);
		if (SkillData)
		{
			AvailableSkillTypes.Add(SkillData->SkillType);
		}
	}

	for (const int32 SkillID : SkillIDList)
	{
		const FPGSkillDataRow* SkillData = DTManager->GetSkillDataRowByKey(SkillID);
		if (!SkillData) continue;
	
		const float Priority = CalculateSkillPriority(SkillData->SkillType, DistanceToTarget, CurrentHPRatio, Enemy, BlackboardComp, AvailableSkillTypes);
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

float UPGBTService_SelectSkill::CalculateSkillPriority(
	EPGSkillType SkillType, 
	float DistanceToTarget, 
	float CurrentHPRatio, 
	APGCharacterEnemy* Enemy, 
	UBlackboardComponent* BlackboardComp,
	const TSet<EPGSkillType>& AvailableSkillTypes) const
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
			// 힐 스킬이 없으면 체크 생략
			if (!AvailableSkillTypes.Contains(EPGSkillType::Heal))
			{
				return 0.f;
			}
			
			// 1. 아군 중 회복 필요한 대상 체크
			if (Enemy && CheckNeedHealAlly(Enemy))
			{
				return 5.f; // 최우선
			}
			
			// 2. 자신의 HP 체크 (백업)
			if (CurrentHPRatio <= HealSkillHPThreshold)
			{
				return 3.f;
			}
			
			return 0.1f; // HP가 충분하면 낮은 우선순위
		}
		
	case EPGSkillType::SummonEnemy:
		{
			// 소환 스킬이 없으면 체크 생략
			if (!AvailableSkillTypes.Contains(EPGSkillType::SummonEnemy))
			{
				return 0.f;
			}
			
			// 소환 횟수 제한 체크
			if (BlackboardComp)
			{
				const int32 CurrentSummonCount = BlackboardComp->GetValueAsInt(SummonCountKey.SelectedKeyName);
				
				if (CurrentSummonCount >= MaxSummonCountPerBattle)
				{
					return 0.f; // 사용 불가
				}
			}
			
			return 1.5f; // 소환: 기본 우선순위
		}
		
	default:
		return 1.f;
	}
}

bool UPGBTService_SelectSkill::CheckNeedHealAlly(APGCharacterEnemy* Enemy) const
{
	if (!Enemy || !Enemy->GetWorld())
	{
		return false;
	}
	
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(
		Enemy->GetWorld(), 
		APGCharacterEnemy::StaticClass(), 
		FoundActors
	);
	
	for (AActor* Actor : FoundActors)
	{
		APGCharacterEnemy* Ally = Cast<APGCharacterEnemy>(Actor);
		if (!Ally || Ally == Enemy) continue;
		
		// 거리 체크
		const float Distance = FVector::Dist(Enemy->GetActorLocation(), Ally->GetActorLocation());
		if (Distance > AllySearchRadius) continue;
		
		// HP 체크
		const UPGEnemyStatComponent* StatComp = Ally->GetEnemyStatComponent();
		if (StatComp)
		{
			const float HPRatio = StatComp->CurrentHP / StatComp->MaxHP;
			if (HPRatio <= AllyHealThreshold)
			{
				return true; // 회복이 필요한 아군 발견
			}
		}
	}
	
	return false;
}

bool UPGBTService_SelectSkill::HasSkillType(int32 EnemyTID, EPGSkillType SkillType) const
{
	// 캐시 확인
	if (CachedEnemySkillTypes.Contains(EnemyTID))
	{
		return CachedEnemySkillTypes[EnemyTID].Contains(SkillType);
	}
	
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
	
	CachedEnemySkillTypes.Add(EnemyTID, SkillTypes);
	return SkillTypes.Contains(SkillType);
}
