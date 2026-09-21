// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PGAbilitySystem/Abilities/PGEnemyGameplayAbility.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
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

protected:
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    void BeginElitePattern(const FPGSkillDataRow& Row);
    void StrikeElitePattern();
    void FinishElitePattern();
    UPROPERTY(Transient) TObjectPtr<class UDecalComponent> Telegraph;
    UPROPERTY(Transient) FPGSkillDataRow EliteData;
    FTimerHandle PatternTimer;
    FVector StrikeCenter = FVector::ZeroVector;
    uint8 SavedMovementMode = 0;
    bool bElitePattern = false;

	UFUNCTION()
	void OnGameplayEventReceived(FGameplayEventData Payload);
	
};
