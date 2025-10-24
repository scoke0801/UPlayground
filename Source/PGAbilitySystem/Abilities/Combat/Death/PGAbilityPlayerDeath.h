// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGAbilityDeath.h"
#include "PGAbilityPlayerDeath.generated.h"

/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGAbilityPlayerDeath : public UPGAbilityDeath
{
	GENERATED_BODY()

protected:
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
};
