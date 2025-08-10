

#pragma once

#include "CoreMinimal.h"
#include "PGGameplayAbility.h"

#include "PGPlayerGameplayAbility.generated.h"

class APGCharacterPlayer;
/**
 * 플레이어 전용 게임플레이 어빌리티 클래스
 * 플레이어 캐릭터와 관련된 특화된 기능을 제공
 */
UCLASS()
class PGABILITYSYSTEM_API UPGPlayerGameplayAbility : public UPGGameplayAbility
{
	GENERATED_BODY()

private:
	/**
	 * 캐시된 플레이어 캐릭터 참조
	 */
	TWeakObjectPtr<APGCharacterPlayer> CachedPlayerCharacter;
	
public:
	/**
	 * 액터 정보로부터 플레이어 캐릭터를 가져오는 함수
	 */
	UFUNCTION(BlueprintPure, Category="PG|Ability")
	APGCharacterPlayer* GetPlayerCharacterFromActorInfo();
	
	/**
	 * 액터 정보로부터 플레이어 컴뱃 컴포넌트를 가져오는 함수
	 */
	UFUNCTION(BlueprintPure, Category="PG|Ability")
	UPGPlayerCombatComponent* GetPlayerCombatComponentFromActorInfo();

};
