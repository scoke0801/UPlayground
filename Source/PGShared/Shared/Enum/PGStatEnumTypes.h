#pragma once

#include "PGStatEnumTypes.generated.h"

UENUM(BlueprintType)
enum class EPGStatType : uint8
{
	None = 0,

	Health UMETA(DisplayName = "Health", ToolTip = "최대 체력"),
	Defense UMETA(DisplayName = "Defense", ToolTip = "방어력"),
	Attack	UMETA(DisplayName = "Attack", ToolTip = "공격력"),
	CriticalRate	UMETA(DisplayName = "CriticalRate", ToolTip = "치명타 발동 확률"),
	HealAmount	UMETA(DisplayName = "HealAmount", ToolTip = "회복량"),
	
	Max UMETA(Hidden)
};
