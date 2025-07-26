

#pragma once
#include "GameplayTagContainer.h"
#include "AbilitySystem/Abilities/PGPlayerGameplayAbility.h"

#include "PlayerStructTypes.generated.h"

class UInputMappingContext;
class UPGPlayerAnimLayer;

USTRUCT(BlueprintType)
struct FPGPlayerAbilitySet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UPGPlayerGameplayAbility> AbilityToGrant;

	bool IsValid() const;
};


USTRUCT(BlueprintType)
struct FPGPlayerWeaponData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	TSubclassOf<UPGPlayerAnimLayer> WeaponAnimLayerToLink;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	UInputMappingContext* WeaponInputMappingContext;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (TitleProperty = "InputTag"))
	TArray<FPGPlayerAbilitySet> DefaultWeaponAbilies;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	FScalableFloat WeaponBaseDamage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	TSoftObjectPtr<UTexture2D> SoftWeaponIconTexture;
};