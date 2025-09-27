// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "PGAbilityTask_WaitSpawnEnemy.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGWaitSpawnEnemiesDelegate,
	const TArray<AActor*>&, SpawnedEnemies);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPGOnDidNotSpawn);
/**
 * 
 */
UCLASS()
class PGAI_API UPGAbilityTask_WaitSpawnEnemy : public UAbilityTask
{
	GENERATED_BODY()
	
private:
	FGameplayTag CachedEventTag;
	int32 CachedNumToSpawn;
	FVector CachedSpawnOrigin;
	float CachedRandomSpawnRadius;
	
	UPROPERTY(Transient)
	TSoftClassPtr<AActor> CachedSoftEnemyClassToSpawn;

	FDelegateHandle CachedDelegateHandle;

public:
	UPROPERTY(BlueprintAssignable, Category = "PG|AbilityTask")
	FPGWaitSpawnEnemiesDelegate OnSpawnFinished;

	UPROPERTY(BlueprintAssignable, Category = "PG|AbilityTask")
	FPGOnDidNotSpawn DidNotSpawn;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;
	virtual void Activate() override;
	
public:
	UFUNCTION(BlueprintCallable, Category = "PG|AbilityTask", meta = (DisplayName = "Wait Gameplay Event And Spawn Enemy", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", NumToSpawn = "1", RandomSpawnRadius = "200"))
	static UPGAbilityTask_WaitSpawnEnemy* WaitSpawnEnemy(UGameplayAbility* OwningAbility,
		FGameplayTag EventTag, TSoftClassPtr<AActor> SoftClassToSpawn, int32 NumToSpawn,
		const FVector& SpawnOrigin,
		float RandomSpawnRadius);

private:
	void OnGameplayEventReceived(const FGameplayEventData* InPayload);
	void OnEnemyClassLoaded();
};
