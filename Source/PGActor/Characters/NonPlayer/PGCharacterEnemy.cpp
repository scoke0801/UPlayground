// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterEnemy.h"

#include "PGActor/Components/Combat/PGEnemyCombatComponent.h"

UPGPawnCombatComponent* APGCharacterEnemy::GetCombatComponent() const
{
	return CombatComponent;
}
