// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cheat/PGCheatComponent.h"
#include "PGUICheatComponent.generated.h"

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGUICheatComponent : public UPGCheatComponent
{
	GENERATED_BODY()
	
public:
	UFUNCTION(Exec)
	void UICheatTest(bool IsTest);
};
