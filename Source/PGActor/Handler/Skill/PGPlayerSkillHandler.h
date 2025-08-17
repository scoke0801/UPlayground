// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGSkillHandler.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"

/**
 * 
 */
class PGACTOR_API FPGPlayerSkillHandler : public FPGSkillHandler
{
	using Super = FPGSkillHandler;
	
private:
	int32 ComboCount = 0;
	EPGSkillSlot LastUsedSlot = EPGSkillSlot::NormalAttack;
	
public:
	virtual ~FPGPlayerSkillHandler() = default;

public:
	virtual void UseSkill(const EPGSkillSlot InSlotId) override;
	virtual PGSkillId GetSkillID(const EPGSkillSlot InSlotId) override;

private:
	PGSkillId GetNextChainSkillid(const EPGSkillSlot InSlotId) const;
};
