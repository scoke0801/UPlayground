

#pragma once

#include "CoreMinimal.h"
#include "PGPawnCombatComponent.h"
#include "PGPlayerCombatComponent.generated.h"

class APGPlayerWeapon;
/**
 * 플레이어 전용 전투 컴포넌트 클래스
 * 플레이어 캐릭터의 전투 관련 특화된 기능을 제공
 */
UCLASS()
class PGACTOR_API UPGPlayerCombatComponent : public UPGPawnCombatComponent
{
	GENERATED_BODY()
	
};
