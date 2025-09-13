// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityDeath.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "PGActor/Characters/PGCharacterBase.h"

void UPGAbilityDeath::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                      const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                      const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (UAbilityTask_PlayMontageAndWait* Task = PlayMontageWait(GetMontageToPlay()))
	{
		Task->ReadyForActivation();
		
		// TODO - SoundQueue
	}
}

void UPGAbilityDeath::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (APGCharacterBase* Character = Cast<APGCharacterBase>(GetOwningActorFromActorInfo()))
	{
		Character->OnDied();
	}
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
