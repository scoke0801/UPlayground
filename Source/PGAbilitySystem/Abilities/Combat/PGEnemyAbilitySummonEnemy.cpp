// Fill out your copyright notice in the Description page of Project Settings.


#include "PGEnemyAbilitySummonEnemy.h"

#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

void UPGEnemyAbilitySummonEnemy::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                 const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                 const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APGCharacterEnemy* Character = GetEnemyCharacterFromActorInfo();
	if (nullptr == Character)
	{
		EndAbilitySelf();
		return;
	}
	
	FPGSkillDataRow* Row = UPGDataTableManager::Get()->GetRowData<FPGSkillDataRow>(SkillId);
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

}
