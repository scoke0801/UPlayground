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
	friend class FPGComboSequenceTest;
	int32 ComboCount = 0;
	EPGSkillSlot LastUsedSlot = EPGSkillSlot::NormalAttack;
    PGSkillId ComboBaseSkill = INVALID_SKILL_ID;
    double ComboExpiresAt = 0.;
    bool bProfileCombo = false;
    double GetComboTime() const;
    int32 GetComboIndex(EPGSkillSlot Slot, const struct FPGSkillDataRow& BaseSkill, double Now) const;
    void AdvanceCombo(EPGSkillSlot Slot, const struct FPGSkillDataRow& BaseSkill, double Now, float MontageSeconds);
	
public:
	virtual ~FPGPlayerSkillHandler() = default;

public:
	virtual void UseSkill(const EPGSkillSlot InSlotId) override;
    virtual void ResetCombo() override { ComboCount = 0; ComboExpiresAt = 0.; bProfileCombo = false; }
	virtual PGSkillId GetSkillID(const EPGSkillSlot InSlotId) override;

};
