

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PGEnemyGameplayAbility.h"
#include "DataAssets/StartUpData/PGDataAsset_StartUpDataBase.h"
#include "PGDataAsset_EnemyStartUpData.generated.h"

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGDataAsset_EnemyStartUpData : public UPGDataAsset_StartUpDataBase
{
	GENERATED_BODY()


private:
	UPROPERTY(EditDefaultsOnly, Category = "PG|StartUpData")
	TArray<TSubclassOf<UPGEnemyGameplayAbility>> EnemyCombatAbilities;

public:
	virtual void GiveToAbilitySystemComponent(UPGAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1) override;

	
};
