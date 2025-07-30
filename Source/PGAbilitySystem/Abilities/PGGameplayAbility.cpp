


#include "PGGameplayAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGShared/Shared/Enum/PGEnumTypes.h"

void UPGGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CachedSpecHandle = Handle;
	CachedActivationInfo = ActivationInfo;
	CachedActorInfo = ActorInfo;
}

void UPGGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	if (AbilityActivationPolicy == EPGAbilityActivationPolicy::OnGiven)
	{
		if (ActorInfo && false == Spec.IsActive())
		{
			ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
		}
	}
}

void UPGGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (AbilityActivationPolicy == EPGAbilityActivationPolicy::OnGiven)
	{
		if (ActorInfo)
		{
			ActorInfo->AbilitySystemComponent->ClearAbility(Handle);
		}
	}
}

void UPGGameplayAbility::OnMontageCompleted()
{
	EndAbilitySelf();
}

void UPGGameplayAbility::EndAbilitySelf()
{
	EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, true);
}

UPGPawnCombatComponent* UPGGameplayAbility::GetCombatComponentFromActorInfo() const
{
	if (APGCharacterBase* Character = Cast<APGCharacterBase>(GetOwningActorFromActorInfo()))
	{
		return Character->GetCombatComponent();
	}

	return nullptr;
}

APGPlayerController* UPGGameplayAbility::GetPlayerControllerFromActorInfo() const
{
	return Cast<APGPlayerController>(CurrentActorInfo->PlayerController);
}

UPGAbilitySystemComponent* UPGGameplayAbility::GetPGAbilitySystemComponentFromActorInfo() const
{
	return Cast<UPGAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent);
}

FActiveGameplayEffectHandle UPGGameplayAbility::BP_ApplyEffectSpecHandleToTarget(AActor* TargetActor,
	const FGameplayEffectSpecHandle& InSpecHandle, EPGSuccessType& OutSuccessType)
{
	FActiveGameplayEffectHandle Handle = NativeApplyEffectSpecHandleToTarget(TargetActor, InSpecHandle);

	OutSuccessType = Handle.WasSuccessfullyApplied() ? EPGSuccessType::Successful : EPGSuccessType::Failed;

	return Handle;
}

FActiveGameplayEffectHandle UPGGameplayAbility::NativeApplyEffectSpecHandleToTarget(AActor* TargetActor,
                                                                                    const FGameplayEffectSpecHandle& InSpecHandle)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

	check(TargetASC && InSpecHandle.IsValid());
	
	return GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
		*InSpecHandle.Data,
		TargetASC
		);
}
