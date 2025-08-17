// Fill out your copyright notice in the Description page of Project Settings.


#include "PGSkillHandler.h"

#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

FPGSkillData::FPGSkillData(const PGSkillId InSkillId)
{
	Init(InSkillId);
}

void FPGSkillData::Init(const PGSkillId InSkillId)
{
	if (UPGDataTableManager* DataTableManager = UPGDataTableManager::Get())
	{
		if (FPGSkillDataRow* SkillData = DataTableManager->GetRowData<FPGSkillDataRow>(InSkillId))
		{
			SkillId = InSkillId;
			CoolTime = SkillData->SkillCoolTime;
			//LastSkillUsedTime = FPlatformTime::Seconds();
		}
	}
}

FPGSkillHandler::~FPGSkillHandler()
{
}

void FPGSkillHandler::Initialize()
{
	FPGHandler::Initialize();
}

void FPGSkillHandler::Finalize()
{
	FPGHandler::Finalize();
}

void FPGSkillHandler::AddSkill(const EPGSkillSlot InSlotId, const PGSkillId InSkillId)
{
	if (SkillDataMap.Contains(InSlotId))
	{
		return;
	}

	SkillDataMap.Emplace(InSlotId, FPGSkillData(InSkillId));
}

void FPGSkillHandler::RemoveSkill(const EPGSkillSlot InSlotID)
{
	SkillDataMap.Remove(InSlotID);
}

int32 FPGSkillHandler::GetSkillID(const EPGSkillSlot InSlotId)
{
	if (FPGSkillData* Data = SkillDataMap.Find(InSlotId))
	{
		return Data->SkillId;
	}

	return INVALID_SKILL_ID;
}

bool FPGSkillHandler::IsCanUseSkill(const EPGSkillSlot InSlotId)
{
	if (FPGSkillData* Data = SkillDataMap.Find(InSlotId))
	{
		return Data->CoolTime + Data->LastSkillUsedTime < FPlatformTime::Seconds();
	}

	return false;
}
