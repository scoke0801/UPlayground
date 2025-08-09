// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGWeaponBase.h"
#include "PGPlayerWeapon.generated.h"

/**
 * 
 */
UCLASS()
class PGACTOR_API APGPlayerWeapon : public APGWeaponBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "WeaponData")
	FPGWeaponData WeaponData;
	
};
