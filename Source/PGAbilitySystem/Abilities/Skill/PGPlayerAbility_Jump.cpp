// Fill out your copyright notice in the Description page of Project Settings.


#include "PGPlayerAbility_Jump.h"

#include "Abilities/Async/AbilityAsync_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGShared/Shared/Debug/PGDebugHelper.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"

void UPGPlayerAbility_Jump::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                           const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                           const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(GetOwningActorFromActorInfo());
	if (nullptr == Player)
	{
		EndAbilitySelf();
		return;
	}

	Player->SetIsJump(true);
	Player->Jump();

	// 점프 애니메이션이 끝나면 어빌리티 종료 처리
	UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			PGGamePlayTags::Player_Event_JumpLanded);
	if (nullptr != WaitEventTask)
	{
		WaitEventTask->EventReceived.AddDynamic(this, &ThisClass::OnGameplayEventReceived);
		
		WaitEventTask->ReadyForActivation();
	}
}

bool UPGPlayerAbility_Jump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (false == Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(GetOwningActorFromActorInfo());
	if (nullptr != Player)
	{
		return Player->IsCanJump();
	}

	return false;
}

void UPGPlayerAbility_Jump::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(GetOwningActorFromActorInfo());
	if (nullptr != Player)
	{
		// 외부에서 강제적으로 취소 되는 경우를 대비
		Player->SetIsJump(false);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPGPlayerAbility_Jump::OnGameplayEventReceived(FGameplayEventData Payload)
{
	APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(GetOwningActorFromActorInfo());
	if (nullptr != Player)
	{
		Player->SetIsJump(false);
	}

	EndAbilitySelf();
}
