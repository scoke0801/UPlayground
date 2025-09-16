


#include "PGGameplayAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
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

UAbilityTask_PlayMontageAndWait* UPGGameplayAbility::PlayMontageWait(UAnimMontage* MontageToPlay)
{
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageToPlay);
	if (nullptr == MontageTask)
	{
		EndAbilitySelf();
		return nullptr;
	}
	
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageCompleted);

	return MontageTask;
}

void UPGGameplayAbility::PlayMontageNoWait(UAnimMontage* MontageToPlay)
{
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

APGCharacterBase* UPGGameplayAbility::GetCharacter() const
{
	return Cast<APGCharacterBase>(GetOwningActorFromActorInfo());
}

bool UPGGameplayAbility::CheckMontageIsPlaying(APGCharacterBase* Character, float CheckRatio) const
{
	// 현재 실행 중인 몽타주의 재생시간이 20% 이상 남았다면 스킬 사용 제한
	if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			if (UAnimMontage* ActivatedMontage = AnimInstance->GetCurrentActiveMontage())
			{
				float CurrentPosition = AnimInstance->Montage_GetPosition(ActivatedMontage);
				float MontageLength = ActivatedMontage->GetPlayLength();
				float PlayRate = AnimInstance->Montage_GetPlayRate(ActivatedMontage);
				
				if (PlayRate > 0.0f) // 정방향 재생 중인 경우
				{
					float RemainingTime = (MontageLength - CurrentPosition) / PlayRate;
					float TotalTime = MontageLength / PlayRate;
					float RemainingRatio = RemainingTime / TotalTime;
					
					// 남은 재생시간이 {}% 이상이면 스킬 사용 제한
					return RemainingRatio < CheckRatio;
				}
			}
		}
	}

	return true;
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
