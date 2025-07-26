

#pragma once

#include "CoreMinimal.h"
#include "PGActor/Components/PGPawnExtensionComponentBase.h"
#include "PGPawnCombatComponent.generated.h"

class APGWeaponBase;

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

/**
 * 폰의 전투 관련 기능을 담당하는 컴포넌트 클래스
 * 무기 관리, 충돌 처리, 전투 액션 등을 제공
 */
UCLASS()
class PGACTOR_API UPGPawnCombatComponent : public UPGPawnExtensionComponentBase
{
	GENERATED_BODY()


	
};
