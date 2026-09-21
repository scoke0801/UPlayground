// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/PGGameModeStage.h"
#include "PGActor/Progression/PGProfileSubsystem.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Stage/PGStageDataRow.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/PGAtrributeSet.h"
#include "PGShared/Shared/Tag/PGGamePlayStatusTags.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGActor/Manager/PGStageManager.h"

APGGameModeStage::APGGameModeStage()
	:Super()
{
	// 기본 스테이지 매니저 클래스 설정
	StageManagerClass = APGStageManager::StaticClass();
	StageManager = nullptr;
}

void APGGameModeStage::BeginPlay()
{
	Super::BeginPlay();
	
	// 스테이지 매니저 생성
	CreateStageManager();
}

void APGGameModeStage::StartPlay()
{
	Super::StartPlay();
	
	// 게임 시작
	StartGame();
}

void APGGameModeStage::CreateStageManager()
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
		StageManager->OnStageStarted.AddDynamic(this, &APGGameModeStage::OnStageStarted);
		StageManager->OnStageCompleted.AddDynamic(this, &APGGameModeStage::OnStageCompleted);
		StageManager->OnAllMonstersKilled.AddDynamic(this, &APGGameModeStage::OnAllMonstersKilled);
		StageManager->OnEnemySpawned.AddDynamic(this, &APGGameModeStage::OnEnemySpawned);
	}
}

void APGGameModeStage::StartGame()
{
	// 블루프린트 이벤트 호출
	OnGameStarted();
	
	// 첫 번째 스테이지 시작
	if (StageManager)
	{
		int32 Stage = 1;
        if (auto* Profile = UPGProfileSubsystem::Get(this))
        {
            Stage = Profile->GetProfile()->Checkpoint;
            if (!PGData()->GetRowData<FPGStageDataRow>(Stage))
            {
                if (!Profile->BeginNewRun()) return;
                Stage = 1;
            }
        }
        StageManager->StartStage(Stage);
#if !UE_BUILD_SHIPPING
        if (auto* Profile = UPGProfileSubsystem::Get(this)) if (Profile->RetryProbeRemaining >= 0)
        {
            FTimerHandle Timer;
            GetWorldTimerManager().SetTimer(Timer, this, &ThisClass::RunRetryProbe, 0.5f, false);
        }
#endif
	}
}

void APGGameModeStage::EndGame()
{
	// 블루프린트 이벤트 호출
	OnGameEnded();
	
	// TODO: 게임 종료 처리 (점수 저장, 메인 메뉴로 이동 등...)
}

void APGGameModeStage::OnStageStarted(int32 StageNumber)
{
	// 블루프린트 이벤트 호출
	OnStageStartedEvent(StageNumber);
}

void APGGameModeStage::OnStageCompleted(int32 StageNumber)
{
	// 블루프린트 이벤트 호출
	OnStageCompletedEvent(StageNumber);
}

void APGGameModeStage::OnAllMonstersKilled()
{
	// 블루프린트 이벤트 호출
	OnAllMonstersKilledEvent();
}

void APGGameModeStage::OnEnemySpawned(APGCharacterEnemy* SpawnedEnemy)
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

void APGGameModeStage::RunRetryProbe()
{
#if !UE_BUILD_SHIPPING
    auto* Profile = UPGProfileSubsystem::Get(this);
    auto* Character = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Profile || !Character || !StageManager) return;
    auto* ASC = Character->GetPGAbilitySystemComponent();
    const bool bHealthy = ASC->GetHealth() > 0 && !ASC->HasMatchingGameplayTag(PGGamePlayTags::Shared_Status_Dead)
        && Character->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision
        && !Character->GetController()->IsMoveInputIgnored() && StageManager->GetCurrentStageState() == EPGStageState::InProgress;
    if (!bHealthy) ++Profile->RetryProbeFailures;
    UE_LOG(LogTemp, Display, TEXT("PGRetryProbe remaining=%d healthy=%d failures=%d"), Profile->RetryProbeRemaining, bHealthy, Profile->RetryProbeFailures);
    if (Profile->RetryProbeRemaining == 0)
    {
        Profile->RetryProbeRemaining = -1;
        UE_LOG(LogTemp, Display, TEXT("PGRetryProbe COMPLETE failures=%d"), Profile->RetryProbeFailures);
        FPlatformMisc::RequestExit(false); return;
    }
    --Profile->RetryProbeRemaining;
    ASC->SetNumericAttributeBase(UPGAtrributeSet::GetCurrentHealthAttribute(), 0.f);
    if (StageManager->GetCurrentStageState() != EPGStageState::Failed) ++Profile->RetryProbeFailures;
    FTimerHandle Timer;
    GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this](){ if (StageManager) StageManager->RestartRun(); }), 0.1f, false);
#endif
}
