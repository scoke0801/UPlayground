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

protected:
	UPROPERTY(Transient)
	TArray<TSoftObjectPtr<UPGCheatComponent>> _components;

private:
	virtual void BeginDestroy() override;
	
	virtual void InitCheatManager() override;
	
	virtual bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor) override;


private:
	void RegisterComponents(UPGCheatComponent* NewComponent);
};
