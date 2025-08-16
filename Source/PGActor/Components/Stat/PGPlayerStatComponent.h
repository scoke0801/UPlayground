// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGStatComponent.h"
#include "PGPlayerStatComponent.generated.h"

/**
 * 
 */
UCLASS()
class PGACTOR_API UPGPlayerStatComponent : public UPGStatComponent
{
	GENERATED_BODY()

public:
	int32 Level = 1;

public:
	virtual void InitData(int32 CharacterTID) override;
};
