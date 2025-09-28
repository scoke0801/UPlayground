// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "PGSkillHandler.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"

/**
 * 
 */
class PGACTOR_API FPGEnemySkillHandler : public FPGSkillHandler
{
	using Super = FPGSkillHandler;

public:
	virtual ~FPGEnemySkillHandler() = default;

public:
	virtual void UseSkill(const EPGSkillSlot InSlotId) override;

	
	EPGSkillSlot GetSkillSlotByTag(const FGameplayTagContainer& GameplayTags);

private:
	EPGSkillSlot GetRandomSkillSlotBySkillType(const EPGSkillType InSkillType) const;
	
};
