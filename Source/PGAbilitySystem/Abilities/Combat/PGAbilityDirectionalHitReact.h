// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGAbilityHitReact.h"
#include "PGAbilityDirectionalHitReact.generated.h"

/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGAbilityDirectionalHitReact : public UPGAbilityHitReact
{
	GENERATED_BODY()

private:
	int32 CachedMontageIndex = 0;
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual UAnimMontage* GetMontageToPlay() const override;

private:
	void ComputeHitReactDirection(const AActor* Attacker);
};
