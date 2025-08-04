// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGAbilitySkill.h"
#include "PGAbilitySkill_Roll.generated.h"

class APGCharacterBase;
class UMotionWarpingComponent;
/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGAbilitySkill_Roll : public UPGAbilitySkill
{
	GENERATED_BODY()


protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Ability")
	FName RollingDirectionName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Ability")
	FName RollTargetLocationName;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Ability")
	FScalableFloat RollingDistanceScalableFloat;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Ability")
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;

private:
	FVector RollingDirection;

	TWeakObjectPtr<APGCharacterBase> CachedCharacter;
	TWeakObjectPtr<UMotionWarpingComponent> CachedMotionWarpingComponent;
	
public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	bool ComputeRollDirection();
	bool ComputeRollDistance();
};
