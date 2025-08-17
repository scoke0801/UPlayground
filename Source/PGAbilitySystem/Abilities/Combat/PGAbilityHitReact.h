// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGAbilitySystem/Abilities/PGGameplayAbility.h"
#include "PGAbilityHitReact.generated.h"

/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGAbilityHitReact : public UPGGameplayAbility
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Ability")
	TArray<FSoftObjectPath> MontagePaths;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Ability")
	bool FaceToTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PG|Ability")
	FName MaterialParameterName;
	
public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual UAnimMontage* GetMontageToPlay() const;
	
private:
	void FaceToAttacker(const AActor* Attacker);

	void SetHitFxSwitchParameter(float Value);
};
