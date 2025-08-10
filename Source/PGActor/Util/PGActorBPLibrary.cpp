// Fill out your copyright notice in the Description page of Project Settings.


#include "PGActorBPLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGActor/Characters/PGCharacterBase.h"

UPGPawnCombatComponent* UPGActorBPLibrary::NativeGetCombatComponentFromActor(AActor* InActor)
{
	if (APGCharacterBase* Character = Cast<APGCharacterBase>(InActor))
	{
		return Character->GetCombatComponent();
	}
	return nullptr;
}

void UPGActorBPLibrary::BP_DoesActorHavTag(AActor* InActor, FGameplayTag TagToCheck, EPGConfirmType& OutConfirmType)
{
	OutConfirmType = UPGAbilityBPLibrary::NativeDoesActorHaveTag(InActor, TagToCheck) ?
		EPGConfirmType::Yes : EPGConfirmType::
			No;
}

UPGPawnCombatComponent* UPGActorBPLibrary::BP_GetPawnCombatComponentFromActor(AActor* InActor,
	EPGValidType& OutValidType)
{	UPGPawnCombatComponent* CombatComponent = NativeGetCombatComponentFromActor(InActor);
 
 	OutValidType = CombatComponent ? EPGValidType::Valid : EPGValidType::Invalid;
 	return CombatComponent;
}
