// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGAbilityDeath.h"
#include "PGAbilityPlayerDeath.generated.h"

/**
 * 
 */
UCLASS()
class PGABILITYSYSTEM_API UPGAbilityPlayerDeath : public UPGAbilityDeath
{
	GENERATED_BODY()

protected:
	virtual UAnimMontage* GetMontageToPlay() const override;
};
