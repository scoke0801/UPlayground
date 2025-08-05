

#pragma once

#include "CoreMinimal.h"
#include "PGPawnCombatComponent.h"
#include "PGEnemyCombatComponent.generated.h"

/**
 * 적 전용 전투 컴포넌트 클래스
 * 적 캐릭터의 전투 관련 특화된 기능을 제공
 */
UCLASS()
class PGACTOR_API UPGEnemyCombatComponent : public UPGPawnCombatComponent
{
	GENERATED_BODY()

public:
	virtual void OnHitTargetActor(AActor* HitActor) override;
};
