// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cheat/PGCheatComponent.h"
#include "PGObjectCheatComponent.generated.h"

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGObjectCheatComponent : public UPGCheatComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec)
	void ObjectCheatTest(bool IsTest);
};
