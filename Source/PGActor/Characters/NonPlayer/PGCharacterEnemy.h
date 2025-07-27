// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGCharacterEnemy.generated.h"
class UPGEnemyCombatComponent;
/**
 * 
 */
UCLASS()
class PGACTOR_API APGCharacterEnemy : public APGCharacterBase
{
	GENERATED_BODY()

protected:
	/** 컴뱃 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= "PG|Combat", meta = (AllowPrivateAccess = true))
	UPGEnemyCombatComponent* CombatComponent;
	
public:
	
	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	UPGEnemyCombatComponent* GetEnemyCombatComponent() const {return CombatComponent;}

	UFUNCTION(BlueprintPure, Category = "PG|Combat")
	virtual UPGPawnCombatComponent* GetCombatComponent() const override;
	
};
