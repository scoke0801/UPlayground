// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGActor/Components/PGPawnExtensionComponentBase.h"
#include "PGStatComponent.generated.h"

/**
 * 
 */
UCLASS()
class PGACTOR_API UPGStatComponent : public UPGPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	int32 MaxHP = 0;
	int32 CurrentHP = 0;

public:
	virtual void InitData(int32 CharacterTID);
};
