// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/PGAnimNotify_Heal.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGActor/Components/Stat/PGStatComponent.h"

UPGAnimNotify_Heal::UPGAnimNotify_Heal()
{
	SkillTargetActorKey.SelectedKeyName = FName("SkillTargetActor");
}

void UPGAnimNotify_Heal::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	APGCharacterBase* Character = Cast<APGCharacterBase>(MeshComp->GetOwner());
	if (nullptr == Character)
	{
		return;
	}
	AAIController* AIController = Cast<AAIController>(Character->GetInstigatorController());
	if (nullptr == AIController)
	{
		return;
	}

	UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();
	if (nullptr == Blackboard)
	{
		return;
	}

	// 블랙보드에서 타겟 액터 가져오기
	APGCharacterBase* TargetActor = Cast<APGCharacterBase>(Blackboard->GetValueAsObject(SkillTargetActorKey.SelectedKeyName));
	if (nullptr == TargetActor)
	{
		return;
	}

	// 타겟의 StatComponent 가져오기
	UPGStatComponent* StatComponent = Character->GetStatComponent();
	if (nullptr == StatComponent)
	{
		return;
	}

	// 힐 적용
	TargetActor->OnHeal(StatComponent, StatComponent->CalculateHealAmount());
}
