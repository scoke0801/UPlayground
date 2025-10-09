// Fill out your copyright notice in the Description page of Project Settings.

#include "PGBTTask_ExecuteSkill.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGEnemyCombatComponent.h"
#include "PGActor/Components/Stat/PGEnemyStatComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

UPGBTTask_ExecuteSkill::UPGBTTask_ExecuteSkill()
{
	NodeName = TEXT("Execute Skill");
	bNotifyTick = false;
	
	SelectedSkillIDKey.SelectedKeyName = FName("SelectedSkillID");
	TargetActorKey.SelectedKeyName = FName("TargetActor");
	SummonCountKey.SelectedKeyName = FName("SummonCount");
	
	CachedSkillType = EPGSkillType::None;
}

EBTNodeResult::Type UPGBTTask_ExecuteSkill::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APGCharacterEnemy* Enemy = Cast<APGCharacterEnemy>(AIController->GetPawn());
	if (!Enemy)
	{
		return EBTNodeResult::Failed;
	}

	UPGAbilitySystemComponent* ASC = Enemy->GetPGAbilitySystemComponent();
	if (!ASC)
	{
		return EBTNodeResult::Failed;
	}

	// Blackboard에서 스킬 ID 가져오기
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	const int32 SkillID = BlackboardComp->GetValueAsInt(SelectedSkillIDKey.SelectedKeyName);
	
	if (SkillID <= 0)
	{
		return EBTNodeResult::Failed;
	}

	UPGDataTableManager* DTManager = UPGDataTableManager::Get();
	if (!DTManager)
	{
		return EBTNodeResult::Failed;
	}

	const FPGSkillDataRow* SkillData = DTManager->GetSkillDataRowByKey(SkillID);
	if (!SkillData)
	{
		return EBTNodeResult::Failed;
	}

	// 스킬 타입 캐싱 (완료 시 카운트 증가용)
	CachedSkillType = SkillData->SkillType;

	// 힐 스킬인 경우 최적의 타겟 선택
	if (SkillData->SkillType == EPGSkillType::Heal)
	{
		AActor* BestHealTarget = SelectBestHealTarget(Enemy, BlackboardComp);
		if (BestHealTarget)
		{
			BlackboardComp->SetValueAsObject(TargetActorKey.SelectedKeyName, BestHealTarget);
		}
	}

	// 스킬 타입을 GameplayTag로 변환
	FGameplayTag AbilityTag = GetAbilityTagFromSkillType(SkillData->SkillType);
	if (!AbilityTag.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	// AbilitySystemComponent를 통해 어빌리티 실행
	CachedOwnerComp = &OwnerComp;
	
	const bool bActivated = ASC->TryActivateAbilityByTag(AbilityTag);
	if (bActivated)
	{
		// Strafe 이어서 진행할 지 랜덤하게 결정
		BlackboardComp->SetValueAsBool(StrafeKey.SelectedKeyName, CheckExecuteStrafe(SkillData->SkillType));
		
		// 어빌리티 활성화 성공 - 즉시 완료 처리
		// (어빌리티 자체에서 몽타주를 관리함)
		FinishTask(CachedOwnerComp.Get(), EBTNodeResult::Succeeded);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

EBTNodeResult::Type UPGBTTask_ExecuteSkill::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		APGCharacterEnemy* Enemy = Cast<APGCharacterEnemy>(AIController->GetPawn());
		if (Enemy)
		{
			UPGAbilitySystemComponent* ASC = Enemy->GetPGAbilitySystemComponent();
			if (ASC)
			{
				// 현재 활성화된 어빌리티 취소
				FGameplayTag AbilityTag = GetAbilityTagFromSkillType(CachedSkillType);
				if (AbilityTag.IsValid())
				{
					FGameplayTagContainer TagContainer = AbilityTag.GetSingleTagContainer();
					ASC->CancelAbilities(&TagContainer);
				}
			}
		}
	}
	
	// 캐시 정리
	CachedOwnerComp.Reset();
	CachedSkillType = EPGSkillType::None;
	
	return EBTNodeResult::Aborted;
}

void UPGBTTask_ExecuteSkill::FinishTask(UBehaviorTreeComponent* OwnerComp, EBTNodeResult::Type Result)
{
	if (OwnerComp)
	{
		UBlackboardComponent* BB = OwnerComp->GetBlackboardComponent();

		if (BB)
		{
			// 스킬 실행 성공 시 소환 카운트 증가
			if (Result == EBTNodeResult::Succeeded && CachedSkillType == EPGSkillType::SummonEnemy)
			{
				const int32 CurrentCount = BB->GetValueAsInt(SummonCountKey.SelectedKeyName);
				BB->SetValueAsInt(SummonCountKey.SelectedKeyName, CurrentCount + 1);
			}

			// 선택된 스킬 정보 초기화
			BB->SetValueAsInt(SelectedSkillIDKey.SelectedKeyName, 0);
		}
	}
	
	CachedOwnerComp.Reset();
	CachedSkillType = EPGSkillType::None;
}

AActor* UPGBTTask_ExecuteSkill::SelectBestHealTarget(APGCharacterEnemy* Self, UBlackboardComponent* BlackboardComp) const
{
	// TODO: 주변 아군이 notify를 보내고 처리하도록 하는 것이 어떨까?
	if (!Self || !Self->GetWorld())
	{
		return nullptr;
	}
	
	// 현재 타겟 (플레이어)
	AActor* CurrentTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName));
	
	// 자신의 HP 비율
	const UPGEnemyStatComponent* SelfStatComp = Self->GetEnemyStatComponent();
	const float SelfHPRatio = SelfStatComp ? (SelfStatComp->CurrentHP / SelfStatComp->MaxHP) : 1.f;
	
	// 최적의 힐 타겟 찾기
	AActor* BestTarget = Self; // 기본값: 자신
	float LowestHPRatio = SelfHPRatio;
	
	// 주변 아군 탐색
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(Self->GetWorld(), APGCharacterEnemy::StaticClass(), FoundActors);
	
	for (AActor* Actor : FoundActors)
	{
		APGCharacterEnemy* Ally = Cast<APGCharacterEnemy>(Actor);
		if (!Ally || Ally == Self) continue;
		
		// 거리 체크
		const float Distance = FVector::Dist(Self->GetActorLocation(), Ally->GetActorLocation());
		if (Distance > AllySearchRadius) continue;
		
		// HP 체크
		const UPGEnemyStatComponent* AllyStatComp = Ally->GetEnemyStatComponent();
		if (AllyStatComp)
		{
			const float AllyHPRatio = AllyStatComp->CurrentHP / AllyStatComp->MaxHP;
			
			// 가장 HP가 낮은 아군 선택
			if (AllyHPRatio < LowestHPRatio)
			{
				LowestHPRatio = AllyHPRatio;
				BestTarget = Ally;
			}
		}
	}
	
	return BestTarget;
}

FGameplayTag UPGBTTask_ExecuteSkill::GetAbilityTagFromSkillType(EPGSkillType SkillType) const
{
	switch (SkillType)
	{
	case EPGSkillType::Melee:
		return PGGamePlayTags::Enemy_Ability_MeleeSkill;
		
	case EPGSkillType::Projectile:
		return PGGamePlayTags::Enemy_Ability_ProjectileSkill;
		
	case EPGSkillType::AreaOfEffect:
		return PGGamePlayTags::Enemy_Ability_AOESkill;

	case EPGSkillType::Heal:
		return PGGamePlayTags::Enemy_Ability_HealSkill;
		
	case EPGSkillType::SummonEnemy:
		return PGGamePlayTags::Enemy_Ability_SummonSkill;
		
	default:
		return FGameplayTag::EmptyTag;
	}
}

bool UPGBTTask_ExecuteSkill::CheckExecuteStrafe(EPGSkillType SkillType)
{
	switch (SkillType)
	{
	case EPGSkillType::Melee:
		return FMath::FRand() <= 0.3f;
		
	case EPGSkillType::Projectile:
	case EPGSkillType::AreaOfEffect:
		return FMath::FRand() <= 0.75f;
	
	case EPGSkillType::Heal:
		return FMath::FRand() <= 0.7f;
		
	case EPGSkillType::SummonEnemy:
		return FMath::FRand() <= 0.5f;
		
	default:
		break;
	}
	
	return false;
	
}
