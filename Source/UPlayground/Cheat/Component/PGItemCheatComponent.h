// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cheat/PGCheatComponent.h"
#include "PGItemCheatComponent.generated.h"

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGItemCheatComponent : public UPGCheatComponent
{
	GENERATED_BODY()

	
public:
	UFUNCTION(Exec)
	void ItemCheatTest(bool IsTest);
};
