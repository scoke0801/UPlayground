// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/PGGameModeBase.h"
#include "Characters/LocalPlayer/PGLocalPlayerCharacter.h"
#include "Controllers/PGPlayerController.h"

APGGameModeBase::APGGameModeBase()
{
	// 기본 Pawn 클래스 설정
	DefaultPawnClass = APGLocalPlayerCharacter::StaticClass();
	
	// 기본 PlayerController 클래스 설정
	PlayerControllerClass = APGPlayerController::StaticClass();
}

