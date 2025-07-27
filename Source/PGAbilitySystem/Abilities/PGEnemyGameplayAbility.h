

#pragma once

#include "CoreMinimal.h"
#include "PGGameplayAbility.h"
#include "PGEnemyGameplayAbility.generated.h"

class UPGEnemyCombatComponent;
class APGCharacterEnemy;
/**
 * 적 전용 게임플레이 어빌리티 클래스
 * 적 캐릭터와 관련된 특화된 기능을 제공
 */
UCLASS()
class PGABILITYSYSTEM_API UPGEnemyGameplayAbility : public UPGGameplayAbility
{
	GENERATED_BODY()

private:
	/**
	 * 캐시된 적 캐릭터 참조
	 */
	TWeakObjectPtr<APGCharacterEnemy> CachedEmeneyCharacter;
	
public:
	/**
	 * 액터 정보로부터 적 캐릭터를 가져오는 함수
	 */
	UFUNCTION(BlueprintPure, Category = "PG|Ability")
	APGCharacterEnemy* GetEnemyCharacterFromActorInfo();
};
