// Fill out your copyright notice in the Description page of Project Settings.


#include "PGPlayerController.h"

#include "Blueprint/UserWidget.h"

APGPlayerController::APGPlayerController(const FObjectInitializer& ObjectInitializer)
{
	PlayerTeamId = FGenericTeamId(0);
}

ETeamAttitude::Type APGPlayerController::GetTeamAttitudeTowards(const AActor& Other) const
{
	return IGenericTeamAgentInterface::GetTeamAttitudeTowards(Other);
}

void APGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (HUDWidgetClass)
	{
		if (UUserWidget* Widget = CreateWidget(this,HUDWidgetClass))
		{
			Widget->AddToViewport();
		}	
	}
}
