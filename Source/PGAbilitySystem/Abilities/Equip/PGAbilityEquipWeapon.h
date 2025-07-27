// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGAbilitySystem/Abilities/PGGameplayAbility.h"
#include "PGAbilityEquipWeapon.generated.h"


struct my_struct
{
	
};
/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGAbilityEquipWeapon : public UPGGameplayAbility
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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|Ability")
	FGameplayTag WeaponTag;
	
public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnEventReceived(FGameplayEventData Payload);
};
