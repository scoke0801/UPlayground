// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGAbilitySystem/Abilities/PGEnemyGameplayAbility.h"
#include "PGShared/Shared/Define/PGSkillDefine.h"
#include "PGEnemyAbilitySummonEnemy.generated.h"

/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGEnemyAbilitySummonEnemy : public UPGEnemyGameplayAbility
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, Category = "PG|Ability")
	int32 SkillId;
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
