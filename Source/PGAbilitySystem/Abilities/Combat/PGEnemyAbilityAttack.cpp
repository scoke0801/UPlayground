// Fill out your copyright notice in the Description page of Project Settings.


#include "PGEnemyAbilityAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Handler/Skill/PGEnemySkillHandler.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void UPGEnemyAbilityAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                            const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 스킬 결정해서 대상 스킬 Ability를 활성화할 수 있어야한다.
	APGCharacterEnemy* Character = GetEnemyCharacterFromActorInfo();
	if (nullptr == Character)
	{
		EndAbilitySelf();
		return;
	}
	if (false == CheckMontageIsPlaying(Character, 0.2f))
	{
		EndAbilitySelf();
		return;
	}
	
	// Spec에서 태그 확인
	if (FGameplayAbilitySpec* Spec = GetAbilitySystemComponentFromActorInfo()->FindAbilitySpecFromHandle(Handle))
	{
		// DynamicAbilityTags에서 활성화 태그 찾기
		for (const FGameplayTag& Tag : Spec->DynamicAbilityTags)
		{
			UE_LOG(LogTemp, Log, TEXT("Activation tag found: %s"), *Tag.ToString());
			// 원하는 로직 수행
		}
	}
	
	// TODO: 단순히 NormalAttack이 아니라, 스킬을 사용할거라면 스킬 ID를 가져와야 한다
	FPGEnemySkillHandler* SkillHandler = static_cast<FPGEnemySkillHandler*>(Character->GetSkillHandler());
	if (nullptr == SkillHandler)
	{
		EndAbilitySelf();
		return;
	}

	EPGSkillSlot RandomSkillSlot = SkillHandler->GetRandomSkillSlot();
	if (false == SkillHandler->IsCanUseSkill(RandomSkillSlot))
	{
		EndAbilitySelf();
		return;
	}
	FPGSkillDataRow* Row = UPGDataTableManager::Get()->GetRowData<FPGSkillDataRow>(SkillHandler->GetSkillID(RandomSkillSlot));
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
	
	SkillHandler->UseSkill(RandomSkillSlot);
}

void UPGEnemyAbilityAttack::OnGameplayEventReceived(FGameplayEventData Payload)
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