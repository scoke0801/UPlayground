// Fill out your copyright notice in the Description page of Project Settings.


#include "PGPlayerController.h"

APGPlayerController::APGPlayerController(const FObjectInitializer& ObjectInitializer)
{
	PlayerTeamId = FGenericTeamId(0);
}

ETeamAttitude::Type APGPlayerController::GetTeamAttitudeTowards(const AActor& Other) const
{
	return IGenericTeamAgentInterface::GetTeamAttitudeTowards(Other);
}
