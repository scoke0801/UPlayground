// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityPlayerSkill.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGMessage/Dispatcher/PGMessageDispatcher.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void UPGAbilityPlayerSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                      const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                      const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APGCharacterBase* Character = GetCharacter();
	if (nullptr == Character)
	{
		EndAbilitySelf();
		return;
	}

	FPGSkillHandler* SkillHandler = Character->GetSkillHandler();
	if (nullptr == SkillHandler || false == SkillHandler->IsCanUseSkill(SlotIndex))
	{
		EndAbilitySelf();
		return;
	}

	if (false == CheckMontageIsPlaying(Character, 0.2f))
	{
		EndAbilitySelf();
		return;
	}
	
	FPGSkillDataRow* Row = UPGDataTableManager::Get()->GetRowData<FPGSkillDataRow>(SkillHandler->GetSkillID(SlotIndex));
	if(nullptr == Row)
	{
		EndAbilitySelf();
		return;
	}

	UAnimMontage* MontageToPlay = nullptr;
	if (UObject* LoadedObject = Row->MontagePath.TryLoad())
	{
		MontageToPlay = Cast<UAnimMontage>(LoadedObject);
	}
	if (nullptr == MontageToPlay)
	{
		EndAbilitySelf();
	}

	if (UAbilityTask_PlayMontageAndWait* MontageTask = PlayMontageWait(MontageToPlay))
	{
		MontageTask->ReadyForActivation();
	}
	SkillHandler->UseSkill(SlotIndex);

	// TODO: Wait 대신 메시지 기반 구조로 변경가능할 지
	UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		PGGamePlayTags::Shared_Event_Hit);
	if (nullptr != WaitEventTask)
	{
		WaitEventTask->EventReceived.AddDynamic(this, &ThisClass::OnGameplayEventReceived);

		WaitEventTask->ReadyForActivation();
	}
}

void UPGAbilityPlayerSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPGAbilityPlayerSkill::OnGameplayEventReceived(FGameplayEventData Payload)
{
	if (const APGCharacterBase* TargetActor = Cast<APGCharacterBase>(Payload.Target.Get()))
	{
		if (UAbilitySystemComponent* ASC = TargetActor->GetAbilitySystemComponent())
		{
			FGameplayEventData Data;
			Data.Instigator = Payload.Instigator;
			Data.Target = Payload.Target;
			
			ASC->HandleGameplayEvent(PGGamePlayTags::Shared_Event_HitReact, &Data);
		}
	}
}
