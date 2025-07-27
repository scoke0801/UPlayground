

#pragma once
#include "GameplayTagContainer.h"
#include "ScalableFloat.h"
#include "InputMappingContext.h"
#include "Engine/Texture2D.h"
#include "PlayerStructTypes.generated.h"

class UPGGameplayAbility;
class UPGPlayerAnimLayer;
class UPGPlayerGameplayAbility;

USTRUCT(BlueprintType)
struct PGDATA_API FPGPlayerAbilitySet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UPGGameplayAbility> AbilityToGrant;

	bool IsValid() const;
};


USTRUCT(BlueprintType)
struct PGDATA_API FPGPlayerWeaponData
{
	GENERATED_BODY()

public:
	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	// TSubclassOf<UPGPlayerAnimLayer> WeaponAnimLayerToLink;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	UInputMappingContext* WeaponInputMappingContext;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (TitleProperty = "InputTag"))
	TArray<FPGPlayerAbilitySet> DefaultWeaponAbilies;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	FScalableFloat WeaponBaseDamage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly )
	TSoftObjectPtr<UTexture2D> SoftWeaponIconTexture;
};