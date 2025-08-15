#pragma once

#include "PGSkillEnumTypes.generated.h"

UENUM(BlueprintType)
enum class EPGSkillSlot : uint8
{
	NormalAttack = 0,
	
	SkillSlot_1,
	SkillSlot_2,
	SkillSlot_3,
	SkillSlot_4,
	SkillSlot_5,
	SkillSlot_6,	// End Of Player Skill

	SkillSlot_Jump,
	SkillSlot_Dash,
	SkillSlot_Roll,

	// 예약
	SkillSlot_7,
	SkillSlot_8,
	SkillSlot_9,
	SkillSlot_10,
};

UENUM(BlueprintType)
enum class EPGSkillNotifyType : uint8
{
	None = 0,

	Projectile,

	SpawnMonster,

	GuideDecal,
};