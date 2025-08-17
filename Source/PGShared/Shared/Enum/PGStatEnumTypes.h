#pragma once

#include "PGStatEnumTypes.generated.h"

UENUM(BlueprintType)
enum class EPGStatType : uint8
{
	None = 0,

	Hp,

	Max UMETA(Hidden)
};
