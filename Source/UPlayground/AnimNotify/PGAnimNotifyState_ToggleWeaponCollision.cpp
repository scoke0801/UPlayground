// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/PGAnimNotifyState_ToggleWeaponCollision.h"

#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGPawnCombatComponent.h"
#include "PGShared/Shared/Enum/PGEnumTypes.h"

void UPGAnimNotifyState_ToggleWeaponCollision::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                                           float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (APGCharacterBase* Character = Cast<APGCharacterBase>(MeshComp->GetOwner()))
	{
		if (UPGPawnCombatComponent* CombatComponent = Character->GetCombatComponent())
		{
			CombatComponent->ToggleWeaponCollision(true, DamageType);
		}
	}
}

void UPGAnimNotifyState_ToggleWeaponCollision::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (APGCharacterBase* Character = Cast<APGCharacterBase>(MeshComp->GetOwner()))
	{
		if (UPGPawnCombatComponent* CombatComponent = Character->GetCombatComponent())
		{
			CombatComponent->ToggleWeaponCollision(false, DamageType);
		}
	}
}
