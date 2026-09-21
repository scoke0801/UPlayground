// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "PGCheatManager.generated.h"

class UPGCheatComponent;
/**
 * 
 */
UCLASS()
class UPLAYGROUND_API UPGCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
    UFUNCTION(Exec) void PGStageStatus();
    UFUNCTION(Exec) void PGStartStage(int32 StageId = 1);
    UFUNCTION(Exec) void PGCombatStats();
    UFUNCTION(Exec) void PGProfileStatus();
    UFUNCTION(Exec) void PGDropItem(int32 ItemId = 3401, int32 Seed = 1234);
    UFUNCTION(Exec) void PGSaveFailure(bool bFail = true);
    UFUNCTION(Exec) void PGFarmingSmoke();
    UFUNCTION(Exec) void PGFeedbackStatus();
    UFUNCTION(Exec) void PGCombatCycleSmoke();
    UFUNCTION(Exec) void PGStress(int32 EnemyCount = 100, int32 DropCount = 100);
protected:
	UPROPERTY(Transient)
	TArray<TSoftObjectPtr<UPGCheatComponent>> _components;

private:
    FTimerHandle CombatCycleTimer;
    int32 CycleProbeTicks = 0;
    int32 CycleProbeRewards = 0;
    int32 CycleProbeWait = 0;
    float CycleProbeBeforeDamage = 0.f;
    void TickCombatCycleProbe();
	virtual void BeginDestroy() override;
	
	virtual void InitCheatManager() override;
	
	virtual bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor) override;


private:
	void RegisterComponents(UPGCheatComponent* NewComponent);
};
