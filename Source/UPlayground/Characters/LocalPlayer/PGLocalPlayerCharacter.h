// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/PGCharacterBase.h"
#include "PGLocalPlayerCharacter.generated.h"

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API APGLocalPlayerCharacter : public APGCharacterBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
};
