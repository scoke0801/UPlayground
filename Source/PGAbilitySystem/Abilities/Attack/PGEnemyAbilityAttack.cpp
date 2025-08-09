// Fill out your copyright notice in the Description page of Project Settings.


#include "PGEnemyAbilityAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Handler/Skill/PGEnemySkillHandler.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

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

	FPGEnemySkillHandler* SkillHandler = static_cast<FPGEnemySkillHandler*>(Character->GetSkillHandler());
	if (nullptr == SkillHandler || false == SkillHandler->IsCanUseSkill(EPGSkillSlot::NormalAttack))
	{
		EndAbilitySelf();
		return;
	}
	
	FPGSkillDataRow* Row = UPGDataTableManager::Get()->GetRowData<FPGSkillDataRow>(SkillHandler->GetSkillID(EPGSkillSlot::NormalAttack));
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
	SkillHandler->UseSkill(EPGSkillSlot::NormalAttack);
}
