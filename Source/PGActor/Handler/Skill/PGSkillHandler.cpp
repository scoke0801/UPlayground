// Fill out your copyright notice in the Description page of Project Settings.


#include "PGSkillHandler.h"
#include "Engine/World.h"

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

			SkillType = SkillData->SkillType;
			Priority = SkillData->InitialPriority;
		}
	}
}

bool FPGSkillData::IsOnCooldown() const
{
    return CoolTime + LastSkillUsedTime > GetTime();
}

double FPGSkillData::GetTime() const
{
    const auto* World = ClockContext.IsValid() ? ClockContext->GetWorld() : nullptr;
    return World ? World->GetTimeSeconds() : FPlatformTime::Seconds();
}

float FPGSkillHandler::RefundRemainingCooldownByID(int32 SkillID, float Fraction)
{
    for (auto& Pair : SkillDataMap)
        if (Pair.Value.SkillId == SkillID) return RefundRemainingCooldown(Pair.Key, Fraction);
    return 0.f;
}

float FPGSkillHandler::RefundRemainingCooldown(EPGSkillSlot Slot, float Fraction)
{
    auto* Data = GetSkillData(Slot);
    if (!Data || !FMath::IsFinite(Fraction)) return 0.f;
    const float Refund = Data->GetRemainingCooldown() * FMath::Clamp(Fraction, 0.f, 1.f);
    Data->LastSkillUsedTime -= Refund;
    return Refund;
}

float FPGSkillData::GetRemainingCooldown() const
{
	const double CurrentTime = GetTime();
	const double EndTime = LastSkillUsedTime + CoolTime;
	return FMath::Max(0.0f, static_cast<float>(EndTime - CurrentTime));
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
    SkillDataMap[InSlotId].ClockContext = Context;
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

FPGSkillData* FPGSkillHandler::GetSkillData(const EPGSkillSlot InSlotId)
{
	if (false == SkillDataMap.Contains(InSlotId))
	{
		return nullptr;
	}

	return &SkillDataMap[InSlotId];
}

EPGSkillSlot FPGSkillHandler::GetRandomSkillSlot() const
{
	TArray<EPGSkillSlot> KeyList;
	SkillDataMap.GetKeys(KeyList);
    
	return KeyList[FMath::RandRange(0, KeyList.Num() - 1)];
}

bool FPGSkillHandler::IsCanUseSkill(const EPGSkillSlot InSlotId)
{
	if (FPGSkillData* Data = SkillDataMap.Find(InSlotId))
	{
        return !Data->IsOnCooldown();
	}

	return false;
}

bool FPGSkillHandler::IsSkillReadyByID(int32 SkillID) const
{
	for (const auto& Pair : SkillDataMap)
	{
		if (Pair.Value.SkillId == SkillID)
		{
			return !Pair.Value.IsOnCooldown();
		}
	}
	return false; // Unknown IDs must not enter AI candidate lists.
}

float FPGSkillHandler::GetRemainingCooldownByID(int32 SkillID) const
{
	for (const auto& Pair : SkillDataMap)
	{
		if (Pair.Value.SkillId == SkillID)
		{
			return Pair.Value.GetRemainingCooldown();
		}
	}
	return 0.0f;
}

int32 FPGSkillHandler::GetPriorityByID(int32 SkillID) const
{
	for (const auto& Pair : SkillDataMap)
	{
		if (Pair.Value.SkillId == SkillID)
		{
			return Pair.Value.Priority;
		}
	}
	return 1; // 기본값
}

EPGSkillSlot FPGSkillHandler::FindSlotBySkillID(int32 SkillID) const
{
	for (const auto& Pair : SkillDataMap)
	{
		if (Pair.Value.SkillId == SkillID)
		{
			return Pair.Key;
		}
	}
	return EPGSkillSlot::NormalAttack;
}

void FPGSkillHandler::UseSkill(const EPGSkillSlot InSlotId)
{
	if (FPGSkillData* Data = SkillDataMap.Find(InSlotId))
	{
		// 쿨타임 업데이트
        Data->LastSkillUsedTime = FMath::Max(UE_DOUBLE_SMALL_NUMBER, Data->GetTime());
	}
}
