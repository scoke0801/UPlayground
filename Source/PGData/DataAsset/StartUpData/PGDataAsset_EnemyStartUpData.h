

#pragma once

#include "CoreMinimal.h"
#include "PGAbilitySystem/Abilities/PGEnemyGameplayAbility.h"
#include "PGDataAsset_StartUpDataBase.h"
#include "PGDataAsset_EnemyStartUpData.generated.h"

/**
 * 
 */
UCLASS()
class PGDATA_API UPGDataAsset_EnemyStartUpData : public UPGDataAsset_StartUpDataBase
{
	GENERATED_BODY()


private:
	UPROPERTY(EditDefaultsOnly, Category = "PG|StartUpData")
	TArray<TSubclassOf<UPGEnemyGameplayAbility>> EnemyCombatAbilities;

public:
	virtual void GiveToAbilitySystemComponent(UPGAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1) override;

	
};
