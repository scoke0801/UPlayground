

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGDataAsset_StartUpDataBase.generated.h"

class UGameplayEffect;
class UPGAbilitySystemComponent;
class UPGGameplayAbility;
/**
 * 
 */
UCLASS()
class PGDATA_API UPGDataAsset_StartUpDataBase : public UDataAsset
{
	GENERATED_BODY()

public:
	// ApplyLevel :  게임 난이도, 옵션 조절용
	virtual void GiveToAbilitySystemComponent(UPGAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1);

protected:
	void GrantAbilities(const TArray<TSubclassOf<UPGGameplayAbility>>& inAbilitiesToGive,
		UPGAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1);

	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "PG|StartUpData")
	TArray<TSubclassOf<UPGGameplayAbility>> ActivateOnGivenAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "PG|StartUpData")
	TArray<TSubclassOf<UPGGameplayAbility>> ReactiveAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "PG|StartUpData")
	TArray<TSubclassOf<UGameplayEffect>> StartUpGameplayEffects;
};
