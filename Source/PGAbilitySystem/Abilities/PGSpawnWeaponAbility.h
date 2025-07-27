// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGGameplayAbility.h"
#include "PGSpawnWeaponAbility.generated.h"

class APGWeaponBase;
/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGSpawnWeaponAbility : public UPGGameplayAbility
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="PG|Abilities")
	TSubclassOf<APGWeaponBase> WeaponClassToSpawn;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="PG|Abilities")
	FName SocketNameToAttachTo;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="PG|Abilities")
	FGameplayTag WeaponTagToRegister;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="PG|Abilities")
	bool RegsisterAsEquippedWeapon;

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
