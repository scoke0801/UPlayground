// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityDeath.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/ActorAssetPath/PGDeathDataRow.h"

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

UAnimMontage* UPGAbilityDeath::GetMontageToPlay() const
{
	APGCharacterBase* Character = Cast<APGCharacterBase>(GetOwningActorFromActorInfo());
	if (nullptr == Character)
	{
		return nullptr;
	}
	
	FPGDeathDataRow* Data = PGData()->GetRowData<FPGDeathDataRow>(Character->GetCharacterTID());
	if (nullptr == Data)
	{
		return nullptr;
	}

	int32 Index = FMath::RandRange(0, Data->DeathMontagePath.Num() - 1);
	if (Data->DeathMontagePath.IsValidIndex(Index))
	{
		if (UObject* LoadedObject = Data->DeathMontagePath[Index].TryLoad())
		{
			return Cast<UAnimMontage>(LoadedObject);
		}
	}
	return nullptr;
}
