// Fill out your copyright notice in the Description page of Project Settings.

#include "PGBTTask_ExecuteSkill.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Components/Combat/PGEnemyCombatComponent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

UPGBTTask_ExecuteSkill::UPGBTTask_ExecuteSkill()
{
	NodeName = TEXT("Execute Skill");
	bNotifyTick = false;
	
	SelectedSkillIDKey.SelectedKeyName = FName("SelectedSkillID");
	TargetActorKey.SelectedKeyName = FName("TargetActor");
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
	if (!SkillData || SkillData->MontagePath.IsNull())
	{
		return EBTNodeResult::Failed;
	}

	UAnimMontage* SkillMontage = Cast<UAnimMontage>(SkillData->MontagePath.TryLoad());
	if (!SkillMontage)
	{
		return EBTNodeResult::Failed;
	}

	UAnimInstance* AnimInstance = Enemy->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		return EBTNodeResult::Failed;
	}

	// 몽타주 재생
	CachedOwnerComp = &OwnerComp;
	
	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &UPGBTTask_ExecuteSkill::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, SkillMontage);
	
	const float PlayRate = AnimInstance->Montage_Play(SkillMontage);
	if (PlayRate > 0.f)
	{
		return EBTNodeResult::InProgress;
	}

	return EBTNodeResult::Failed;
}

void UPGBTTask_ExecuteSkill::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (CachedOwnerComp.IsValid())
	{
		FinishTask(CachedOwnerComp.Get(), bInterrupted ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
	}
}

void UPGBTTask_ExecuteSkill::FinishTask(UBehaviorTreeComponent* OwnerComp, EBTNodeResult::Type Result)
{
	if (OwnerComp)
	{
		FinishLatentTask(*OwnerComp, Result);
	}
	CachedOwnerComp.Reset();
}
