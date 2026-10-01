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
    friend class FPGAttackPatternLifecycleTest;

public:
    UPGEnemyAbilityAttack();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    void BeginElitePattern(const FPGSkillDataRow& Row);
    void StrikeElitePattern();
    void FinishElitePattern();
    void UpdatePattern();
    void UpdateAim();
    void ShowTelegraph(const FVector& Center, bool bLine, bool bRecovery = false);
    void BeginRecovery();
    UPROPERTY(Transient) TObjectPtr<class UDecalComponent> Telegraph;
    UPROPERTY(Transient) FPGSkillDataRow EliteData;
    FTimerHandle PatternTimer;
    FTimerHandle UpdateTimer;
    TWeakObjectPtr<AActor> PatternTarget;
    TWeakObjectPtr<AActor> ActiveProjectile;
    FVector PatternOrigin = FVector::ZeroVector;
    FVector PatternForward = FVector::ForwardVector;
    FVector HazardOrigin = FVector::ZeroVector;
    double PatternStartedAt = 0;
    double LastUpdateAt = 0;
    float Travelled = 0;
    int32 HazardIndex = 0;
    bool bTravelling = false;
    bool bStriking = false;
    FVector StrikeCenter = FVector::ZeroVector;
    uint8 SavedMovementMode = 0;
    bool bElitePattern = false;

	UFUNCTION()
	void OnGameplayEventReceived(FGameplayEventData Payload);
	
};
