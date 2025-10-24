// Fill out your copyright notice in the Description page of Project Settings.

#include "PGBTService_SelectSkill.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Stat/PGEnemyStatComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
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
	
	// 이미 유효한 값이 설정되어 있는 동안에는 별도 처리를 하지 않습니다.
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (0 != BlackboardComp->GetValueAsInt(SelectedSkillIDKey.SelectedKeyName))
	{
		return;
	}
	
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
	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (nullptr == TargetActor)
	{
		// 로컬 플레이어 캐릭터 가져오기
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			if (APawn* PlayerPawn = PlayerController->GetPawn())
			{
				TargetActor = PlayerPawn;
				BlackboardComp->SetValueAsObject(FName("TargetActor"), TargetActor);
			}
		}
	}
	float DistanceToTarget = -1.f;
	if (TargetActor)
	{
		DistanceToTarget = FVector::Dist(Enemy->GetActorLocation(), TargetActor->GetActorLocation());
	}
	else
	{
		return;
	}
	
	// 현재 HP 비율 계산
	const UPGEnemyStatComponent* StatComp = Enemy->GetEnemyStatComponent();
	const float CurrentHPRatio = StatComp ? (StatComp->CurrentHealth / StatComp->GetStat(EPGStatType::Health)) : 1.f;
	
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
	if (!DTManager || SkillIDList.Num() == 0 || !Enemy)
	{
		return -1;
	}

	// SkillHandler를 통해 쿨타임 체크
	FPGSkillHandler* SkillHandler = Enemy->GetSkillHandler();
	if (!SkillHandler)
	{
		// SkillHandler가 없으면 첫 번째 스킬 반환
		return SkillIDList[0];
	}

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

	// 1단계: 쿨타임이 준비된 스킬 중에서 가중치 기반 선택
	TArray<int32> ReadySkills;
	TArray<float> ReadyWeights;

	for (const int32 SkillID : SkillIDList)
	{
		// SkillHandler를 통해 쿨타임 체크
		if (!SkillHandler->IsSkillReadyByID(SkillID))
		{
			continue;
		}

		const FPGSkillDataRow* SkillData = DTManager->GetSkillDataRowByKey(SkillID);
		if (!SkillData) continue;
	
		const float Priority = CalculateSkillPriority(SkillID, SkillData->SkillType, DistanceToTarget, CurrentHPRatio, Enemy, BlackboardComp, AvailableSkillTypes);
		if (Priority > 0.f)
		{
			ReadySkills.Add(SkillID);
			ReadyWeights.Add(Priority);
		}
	}
	
	// 쿨타임이 준비된 유효한 스킬이 있으면 가중치 기반 선택
	if (ReadySkills.Num() > 0)
	{
		if (ReadyWeights.Num() == 0)
		{
			// 가중치 미사용 시 랜덤 선택
			return ReadySkills[FMath::RandRange(0, ReadySkills.Num() - 1)];
		}
		
		// 가중치 기반 랜덤 선택
		float TotalWeight = 0.f;
		for (float Weight : ReadyWeights)
		{
			TotalWeight += Weight;
		}
		
		const float RandomValue = FMath::FRandRange(0.f, TotalWeight);
		float AccumulatedWeight = 0.f;
		
		for (int32 i = 0; i < ReadySkills.Num(); ++i)
		{
			AccumulatedWeight += ReadyWeights[i];
			if (RandomValue <= AccumulatedWeight)
			{
				return ReadySkills[i];
			}
		}

		return ReadySkills[0];
	}

	// 2단계: 모든 스킬이 쿨타임 중이면, 쿨타임이 가장 짧게 남은 스킬 선택 (무조건 1개 보장)
	int32 BestSkillID = SkillIDList[0];
	float MinCooldown = SkillHandler->GetRemainingCooldownByID(BestSkillID);

	for (int32 i = 1; i < SkillIDList.Num(); ++i)
	{
		const int32 SkillID = SkillIDList[i];
		const float RemainingCooldown = SkillHandler->GetRemainingCooldownByID(SkillID);
		
		if (RemainingCooldown < MinCooldown)
		{
			MinCooldown = RemainingCooldown;
			BestSkillID = SkillID;
		}
	}

	return BestSkillID;
}

float UPGBTService_SelectSkill::CalculateSkillPriority(
	int32 SkillID,
	EPGSkillType SkillType, 
	float DistanceToTarget, 
	float CurrentHPRatio, 
	APGCharacterEnemy* Enemy, 
	UBlackboardComponent* BlackboardComp,
	const TSet<EPGSkillType>& AvailableSkillTypes) const
{
	// SkillHandler에서 Priority 확인
	if (Enemy)
	{
		if (FPGSkillHandler* SkillHandler = Enemy->GetSkillHandler())
		{
			const int32 CurrentPriority = SkillHandler->GetPriorityByID(SkillID);
			
			// Priority가 1이 아니면(설정된 경우), 해당 값을 우선적으로 사용
			if (CurrentPriority != 1)
			{
				return static_cast<float>(CurrentPriority);
			}
		}
	}

	// Priority가 1인 경우(기본값), 거리/HP/타입별 동적 계산 수행
	switch (SkillType)
	{
	case EPGSkillType::Melee:
		{
			// 근접: 가까울수록 우선순위 높음
			if (DistanceToTarget < 0.f) return 1.f;
			return DistanceToTarget <= MeleeSkillMaxRange ? 3.f : 0.5f;
		}
		
	case EPGSkillType::Projectile:
	case EPGSkillType::AreaOfEffect:
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
			const float HPRatio = StatComp->CurrentHealth / StatComp->GetStat(EPGStatType::Health);
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
