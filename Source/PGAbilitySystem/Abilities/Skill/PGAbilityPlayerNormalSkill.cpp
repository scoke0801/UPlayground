// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAbilityPlayerNormalSkill.h"
#include "PGActor/Controllers/PGPlayerController.h"

bool UPGAbilityPlayerNormalSkill::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// PlayerController가 클릭을 처리했는지 확인
	if (ActorInfo && ActorInfo->PlayerController.IsValid())
	{
		if (APGPlayerController* PC = Cast<APGPlayerController>(ActorInfo->PlayerController.Get()))
		{
			if (PC->WasLastClickConsumed())
			{
				// 클릭 가능한 오브젝트를 클릭했으므로 일반 공격 실행 안함
				return false;
			}
		}
	}
	
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}
