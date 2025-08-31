#pragma once

#include "PGProjectileEnumType.generated.h"

// 투사체 타입 열거형
UENUM(BlueprintType)
enum class EPGProjectileType : uint8
{ 
	Arrow       UMETA(DisplayName = "Arrow"),
	Custom      UMETA(DisplayName = "Custom")
};

// 충돌 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPGOnProjectileHit, AActor*, HitActor, const FHitResult&, HitResult);
