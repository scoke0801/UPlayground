// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilitySkill.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void UPGAbilitySkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                      const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                      const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UPGDataTableManager* dataTableManager = GetAvatarActorFromActorInfo()->GetGameInstance()->GetSubsystem<UPGDataTableManager>();
	if (nullptr == dataTableManager)
	{
		EndAbilitySelf();
		return;
	}
	
	FPGSkillDataRow* Row = dataTableManager->GetRowData<FPGSkillDataRow>(1);
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
	
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageToPlay);
	if (nullptr == MontageTask)
	{
		EndAbilitySelf();
		return;
	}
	
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	
	MontageTask->ReadyForActivation();
}
