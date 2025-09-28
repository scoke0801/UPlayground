// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGAbilitySystem/Abilities/PGEnemyGameplayAbility.h"
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
	
	// 소환할 적 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Spawn Settings")
	TSoftClassPtr<APGCharacterEnemy> SoftClassToSpawn;

	// 소환할 적의 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Spawn Settings")
	int32 NumToSpawn = 1;

	// 랜덤 소환 반경
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Spawn Settings")
	float RandomSpawnRadius = 200.0f;

	// 이벤트 태그
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Gameplay Tags")
	FGameplayTag EventTag;
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION()
	void OnSpawnFinished(const TArray<AActor*>& Actors);
	
	UFUNCTION()
	void OnSpawnFailed();
};
