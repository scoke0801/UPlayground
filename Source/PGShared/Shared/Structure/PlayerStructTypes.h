

#pragma once
#include "GameplayTagContainer.h"
#include "ScalableFloat.h"
#include "InputMappingContext.h"
#include "Engine/Texture2D.h"
#include "PlayerStructTypes.generated.h"


class UGameplayAbility;

USTRUCT(BlueprintType)
struct PGSHARED_API FPGPlayerAbilitySet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> AbilityToGrant;

	bool IsValid() const;
};


USTRUCT(BlueprintType)
struct PGSHARED_API FPGWeaponData
{
	GENERATED_BODY()

public:
	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	// TSubclassOf<UPGPlayerAnimLayer> WeaponAnimLayerToLink;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	UInputMappingContext* WeaponInputMappingContext;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (TitleProperty = "InputTag"))
	TArray<FPGPlayerAbilitySet> DefaultWeaponAbilities;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	FScalableFloat WeaponBaseDamage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	TSoftObjectPtr<UTexture2D> SoftWeaponIconTexture;
};