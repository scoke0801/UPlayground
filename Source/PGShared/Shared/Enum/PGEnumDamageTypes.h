#pragma once

#include "PGEnumDamageTypes.generated.h"

/**
 * 데미지 토글 타입을 정의하는 열거형
 */
UENUM(BlueprintType)
enum class EPGToggleDamageType : uint8
{
	CurrentEquippedWeapon,	// 현재 장착 무기
	LeftHand,				// 왼손(맨손)
	RightHand,				// 오른손(맨손)

	LeftFoot,				// 왼발
	RightFoot,				// 오른발

	Tail,					// 꼬리
};

UENUM(BlueprintType)
enum class EPGDamageType : uint8
{
	Normal      UMETA(DisplayName = "Normal"),
	Critical    UMETA(DisplayName = "Critical"),
	Heal        UMETA(DisplayName = "Heal"),
	Miss        UMETA(DisplayName = "Miss")
};