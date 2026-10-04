// Fill out your copyright notice in the Description page of Project Settings.

#include "PGBTService_SelectSkill.h"
#include "PGAI/PGCombatSpatial.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Stat/PGEnemyStatComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
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
	
	// Validate retained choices when the context changes; leave running attacks alone.
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp) return;
	
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
	if (Enemy->bPatternActive || Enemy->bPerformingHeavyAttack ||
		(Enemy->GetPGAbilitySystemComponent() && Enemy->GetPGAbilitySystemComponent()->GetCurrentMontage())) return;
	const int32 PendingID = BlackboardComp->GetValueAsInt(SelectedSkillIDKey.SelectedKeyName);
	if (PendingID > 0)
	{
		const auto* Pending = DTManager->GetSkillDataRowByKey(PendingID);
		auto* Handler = Enemy->GetSkillHandler();
		if (Pending && Pending->MinimumBossPhase <= Enemy->BossPhase && Pending->SelectionWeight > 0.f &&
			Handler && Handler->IsSkillReadyByID(PendingID) &&
			BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName)) return;
		BlackboardComp->SetValueAsInt(SelectedSkillIDKey.SelectedKeyName, 0);
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
	const float CurrentHPRatio = StatComp ? StatComp->GetHealthRatio() : 1.f;
	
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
		return INDEX_NONE;
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
		if (!SkillData || (SkillData->TelegraphDuration > 0.f && !SkillData->IsPatternValid())) continue;
	
        if (SkillData->MinimumBossPhase > Enemy->BossPhase) continue;
		const float Priority = CalculateSkillPriority(SkillID, SkillData->SkillType, DistanceToTarget, CurrentHPRatio, Enemy, BlackboardComp, AvailableSkillTypes)
            * FMath::Max(0, SkillData->InitialPriority) * FMath::Max(0.f, SkillData->SelectionWeight);
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

	// 금지된 소환이나 쿨타임 중인 스킬을 강제로 선택하지 않는다.
    return INDEX_NONE;

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
    // Hard limits must be evaluated before dynamic priority escalation.
    if (SkillType == EPGSkillType::SummonEnemy && BlackboardComp &&
        BlackboardComp->GetValueAsInt(SummonCountKey.SelectedKeyName) >= MaxSummonCountPerBattle) return 0.f;

    // Authored weights multiply context; they never bypass summon or health constraints.
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
    return PGCombatSpatial::FindInjuredAlly(Enemy, AllySearchRadius, AllyHealThreshold, false) != nullptr;
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
