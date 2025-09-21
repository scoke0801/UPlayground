// Fill out your copyright notice in the Description page of Project Settings.


#include "PGEnemySkillHandler.h"

#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

void FPGEnemySkillHandler::UseSkill(const EPGSkillSlot InSlotId)
{
	PGSkillId SkillId = Super::GetSkillID(InSlotId);
	
	if (FPGSkillDataRow* SkillData = UPGDataTableManager::Get()->GetRowData<FPGSkillDataRow>(SkillId))
	{
		if ( 0 < SkillData->ChainSkillIdList.Num() &&
			ComboCount < SkillData->ChainSkillIdList.Num())
		{
			++ComboCount;
		}
		else
		{
			ComboCount = 0;
		}
	}
	else
	{
		ComboCount = 0;
	}

	LastUsedSlot = InSlotId;
	LastUsedTime = FPlatformTime::Seconds();
}

PGSkillId FPGEnemySkillHandler::GetSkillID(const EPGSkillSlot InSlotId)
{
	if (false == SkillDataMap.Contains(InSlotId))
	{
		return INVALID_SKILL_ID;
	}
	
	PGSkillId SkillId = Super::GetSkillID(InSlotId);
	
	if (FPGSkillDataRow* SkillData = UPGDataTableManager::Get()->GetRowData<FPGSkillDataRow>(SkillId))
	{
		if (0 < ComboCount && SkillData->ChainSkillIdList.IsValidIndex(ComboCount - 1))
		{
			return SkillData->ChainSkillIdList[ComboCount - 1];
		}
	}

	return SkillId;
}

EPGSkillSlot FPGEnemySkillHandler::GetSkillSlotByTag(const FGameplayTagContainer& GameplayTags)
{
	for (const FGameplayTag& Tag : GameplayTags)
	{
	}
	return GetRandomSkillSlot();
}
