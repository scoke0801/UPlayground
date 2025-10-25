// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/PGGameModeBase.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "Engine/World.h"

APGGameModeBase::APGGameModeBase()
{
	// 기본 Pawn 클래스 설정
	DefaultPawnClass = APGCharacterPlayer::StaticClass();
	
	// 기본 PlayerController 클래스 설정
	PlayerControllerClass = APGPlayerController::StaticClass();

	// 기본 스테이지 매니저 클래스 설정
	StageManagerClass = APGStageManager::StaticClass();
	StageManager = nullptr;
}

void APGGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	
	// 스테이지 매니저 생성
	CreateStageManager();
	
	// 게임 시작
	StartGame();
}

void APGGameModeBase::CreateStageManager()
{
	if (!StageManagerClass)
	{
		return;
	}
	
	// 스테이지 매니저 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName("PGStageManager");
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	StageManager = GetWorld()->SpawnActor<APGStageManager>(StageManagerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	
	if (StageManager)
	{
		// 스테이지 이벤트 바인딩
		StageManager->OnStageStarted.AddDynamic(this, &APGGameModeBase::OnStageStarted);
		StageManager->OnStageCompleted.AddDynamic(this, &APGGameModeBase::OnStageCompleted);
		StageManager->OnAllMonstersKilled.AddDynamic(this, &APGGameModeBase::OnAllMonstersKilled);
		StageManager->OnEnemySpawned.AddDynamic(this, &APGGameModeBase::OnEnemySpawned);
	}
}

void APGGameModeBase::StartGame()
{
	// 블루프린트 이벤트 호출
	OnGameStarted();
	
	// 첫 번째 스테이지 시작
	if (StageManager)
	{
		StageManager->StartStage(1);
	}
}

void APGGameModeBase::EndGame()
{
	// 블루프린트 이벤트 호출
	OnGameEnded();
	
	// TODO: 게임 종료 처리 (점수 저장, 메인 메뉴로 이동 등...)
}

void APGGameModeBase::OnStageStarted(int32 StageNumber)
{
	// 블루프린트 이벤트 호출
	OnStageStartedEvent(StageNumber);
}

void APGGameModeBase::OnStageCompleted(int32 StageNumber)
{
	// 블루프린트 이벤트 호출
	OnStageCompletedEvent(StageNumber);
}

void APGGameModeBase::OnAllMonstersKilled()
{
	// 블루프린트 이벤트 호출
	OnAllMonstersKilledEvent();
}

void APGGameModeBase::OnEnemySpawned(APGCharacterEnemy* SpawnedEnemy)
{
	if (IsValid(SpawnedEnemy))
	{
		UE_LOG(LogTemp, Log, TEXT("PGGameModeBase: 적 스폰됨 - %s"), *SpawnedEnemy->GetName());
		
		// 적이 죽을 때 스테이지 매니저에 알림을 보내도록 설정
		// 이미 APGCharacterEnemy에 OnDied() 함수가 있으므로, 
		// 그 함수를 오버라이드하거나 델리게이트를 사용해서 연결해야 합니다.
		
		// 블루프린트 이벤트 호출
		OnEnemySpawnedEvent(SpawnedEnemy);
	}
}
