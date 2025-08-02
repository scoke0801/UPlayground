// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilitySkill.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGMessage/Dispatcher/PGMessageDispatcher.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void UPGAbilitySkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                      const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                      const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APGCharacterBase* Character = Cast<APGCharacterBase>(GetOwningActorFromActorInfo());
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
					
					// 남은 재생시간이 20% 이상이면 스킬 사용 제한
					if (RemainingRatio >= 0.2f)
					{
						EndAbilitySelf();
						return;
					}
				}
			}
		}
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
	SkillHandler->UseSkill(SlotIndex);

	UPGMessageDispatcher::Get()->AddUObject(this, &ThisClass::OnMessage);
}