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


/**
 * 데미지 토글 타입을 정의하는 열거형
 */
UENUM(BlueprintType)
enum class EToggleDamageType : uint8
{
	CurrentEquippedWeapon,	// 현재 장착 무기
	LeftHand,				// 왼손(맨손)
	RightHand,				// 오른손(맨손)
};
