// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/PGAnimNotifyState_ComboWindow.h"

#include "Characters/LocalPlayer/PGLocalPlayerCharacter.h"

void UPGAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                                 float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (APGLocalPlayerCharacter* localPlayer = Cast<APGLocalPlayerCharacter>(MeshComp->GetOwner()))
	{
		localPlayer->StartSkillWindow();
	}
}

void UPGAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (APGLocalPlayerCharacter* localPlayer = Cast<APGLocalPlayerCharacter>(MeshComp->GetOwner()))
	{
		localPlayer->EndSkillWindow();
	}
}
