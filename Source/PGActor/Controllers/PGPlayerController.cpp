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

	// 마우스 커서 표시
	bShowMouseCursor = true;
    
	// 입력 모드 설정 (게임과 UI 동시 입력)
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}
