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

