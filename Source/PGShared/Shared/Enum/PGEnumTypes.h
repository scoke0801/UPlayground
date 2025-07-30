#pragma once

#include "PGEnumTypes.generated.h"

UENUM(BlueprintType)
enum class EPGConfirmType : uint8
{
	Yes,
	No
};

UENUM(BlueprintType)
enum class EPGValidType : uint8
{
	Valid,
	Invalid
};

UENUM(BlueprintType)
enum class EPGSuccessType : uint8
{
	Successful,
	Failed
};

UENUM(BlueprintType)
enum class EPGSkillSlot : uint8
{
	NormalAttack = 0,
	SkillSlot_1,
	SkillSlot_2,
	SkillSlot_3,
	SkillSlot_4,
	SkillSlot_5,
	SkillSlot_6,
	
};