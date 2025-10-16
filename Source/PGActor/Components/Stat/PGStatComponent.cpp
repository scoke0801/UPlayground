// Fill out your copyright notice in the Description page of Project Settings.


#include "PGStatComponent.h"

#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Stat/PGCharacterStatDataRow.h"

void UPGStatComponent::InitData(int32 CharacterTID)
{
	// 테이블에서 읽어오는 구조로 나중에 고치고 우선 상수

	if (UPGDataTableManager* Manager = PGData())
	{
		if (FPGCharacterStatDataRow* Data = Manager->GetRowData<FPGCharacterStatDataRow>(CharacterTID))
		{
			StatMap = Data->Stats;
		}
	}
	if (StatMap.Contains(EPGStatType::Health))
	{
		CurrentHealth = StatMap[EPGStatType::Health];
	}
}

int32 UPGStatComponent::GetStat(EPGStatType StatType) const
{
	if (StatMap.Contains(StatType))
	{
		return StatMap[StatType];
	}
	return 0;
}
