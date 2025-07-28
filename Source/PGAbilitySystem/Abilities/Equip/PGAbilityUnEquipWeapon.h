// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGAbilitySystem/Abilities/PGGameplayAbility.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGAbilityUnEquipWeapon.generated.h"

/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGAbilityUnEquipWeapon : public UPGGameplayAbility
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Ability")
	TObjectPtr<UAnimMontage> MontageToPlay;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Ability")
	FName AttachSocketName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Ability")
	EAttachmentRule LocationRule = EAttachmentRule::SnapToTarget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Ability")
	EAttachmentRule RotationRule = EAttachmentRule::KeepRelative;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Ability")
	EAttachmentRule ScaleRule = EAttachmentRule::KeepWorld;
	
public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;


private:
	UFUNCTION()
	void OnEventReceived(FGameplayEventData Payload);
	
	UFUNCTION()
	void OnMontageCompleted();

private:
	void HandleUnEquipWeapon(APGWeaponBase* Weapon);
};
