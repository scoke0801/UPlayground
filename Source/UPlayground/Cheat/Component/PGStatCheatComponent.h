// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cheat/PGCheatComponent.h"
#include "PGStatCheatComponent.generated.h"

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGStatCheatComponent : public UPGCheatComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec)
	void StatCheatTest(bool IsTest);
};
