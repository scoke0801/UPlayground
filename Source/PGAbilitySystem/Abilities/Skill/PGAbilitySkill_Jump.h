// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGAbilitySkill.h"
#include "PGAbilitySkill_Jump.generated.h"

/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGAbilitySkill_Jump : public UPGAbilitySkill
{
	GENERATED_BODY()

public:
	void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
