// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/PGGameModeBase.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Controllers/PGPlayerController.h"

APGGameModeBase::APGGameModeBase()
{
	// 기본 Pawn 클래스 설정
	DefaultPawnClass = APGCharacterPlayer::StaticClass();
	
	// 기본 PlayerController 클래스 설정
	PlayerControllerClass = APGPlayerController::StaticClass();
}

