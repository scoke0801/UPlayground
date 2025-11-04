// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGUIWindow.h"
#include "PGUIWindowRewardSelect.generated.h"

struct FPGStageDataRow;
/**
 * 
 */
UCLASS()
class PGUI_API UPGUIWindowRewardSelect : public UPGUIWindow
{
	GENERATED_BODY()

public:
	void SetRewardId(int StageId);

private:
	void SelectReward(const FPGStageDataRow* InData);
};
