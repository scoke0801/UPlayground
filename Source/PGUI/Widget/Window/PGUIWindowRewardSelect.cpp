// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIWindowRewardSelect.h"

#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Stage/PGStageDataRow.h"

void UPGUIWindowRewardSelect::SetRewardId(int StageId)
{
	if (UPGDataTableManager* TableManager = PGData())
	{
		if (FPGStageDataRow* StageData = TableManager->GetRowData<FPGStageDataRow>(StageId))
		{
			SelectReward(StageData);
		}
	}
}

void UPGUIWindowRewardSelect::SelectReward(const FPGStageDataRow* InData)
{
}
