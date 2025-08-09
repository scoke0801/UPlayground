// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PGAbilitySystem/Abilities/PGEnemyGameplayAbility.h"
#include "PGEnemyAbilityAttack.generated.h"

/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGEnemyAbilityAttack : public UPGEnemyGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
};
