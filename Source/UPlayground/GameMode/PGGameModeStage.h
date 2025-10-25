// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/PGGameModeBase.h"
#include "PGGameModeStage.generated.h"

/**
 * 
 */
UCLASS()
class UPLAYGROUND_API APGGameModeStage : public APGGameModeBase
{
	GENERATED_BODY()
	
private:
	// 스테이지 매니저 인스턴스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PG|Game Management", meta = (AllowPrivateAccess = "true"))
	class APGStageManager* StageManager;
	
	// 스테이지 매니저 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Game Management", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class APGStageManager> StageManagerClass;

public:
	APGGameModeStage();
	
protected:
	virtual void BeginPlay() override;
	
public:
	// 스테이지 매니저 접근 함수
	UFUNCTION(BlueprintCallable, Category = "PG|Game Management")
	class APGStageManager* GetStageManager() const { return StageManager; }
	
	// 게임 시작
	UFUNCTION(BlueprintCallable, Category = "PG|Game Management")
	void StartGame();
	
	// 게임 종료
	UFUNCTION(BlueprintCallable, Category = "PG|Game Management")
	void EndGame();
	

private:
	// 스테이지 매니저 생성
	void CreateStageManager();

protected:
	// 스테이지 이벤트 바인딩
	UFUNCTION()
	void OnStageStarted(int32 StageNumber);
	
	UFUNCTION()
	void OnStageCompleted(int32 StageNumber);
	
	UFUNCTION()
	void OnAllMonstersKilled();

	UFUNCTION()
	void OnEnemySpawned(class APGCharacterEnemy* SpawnedEnemy);

public:
	// 블루프린트용
	UFUNCTION(BlueprintImplementableEvent, Category = "PG|Game Events")
	void OnGameStarted();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "PG|Game Events")
	void OnGameEnded();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "PG|Game Events")
	void OnStageStartedEvent(int32 StageNumber);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "PG|Game Events")
	void OnStageCompletedEvent(int32 StageNumber);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "PG|Game Events")
	void OnAllMonstersKilledEvent();

	UFUNCTION(BlueprintImplementableEvent, Category = "PG|Game Events")
	void OnEnemySpawnedEvent(class APGCharacterEnemy* SpawnedEnemy);
};
