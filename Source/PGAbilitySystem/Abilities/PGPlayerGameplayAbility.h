

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PGGameplayAbility.h"
#include "PGPlayerGameplayAbility.generated.h"

class UPGPlayerCombatComponent;
class APGPlayerController;
class APGPlayerCharacter;
/**
 * 플레이어 전용 게임플레이 어빌리티 클래스
 * 플레이어 캐릭터와 관련된 특화된 기능을 제공
 */
UCLASS()
class UPLAYGROUND_API UPGPlayerGameplayAbility : public UPGGameplayAbility
{
	GENERATED_BODY()

public:
	/**
	 * 액터 정보로부터 플레이어 캐릭터를 가져오는 함수
	 */
	UFUNCTION(BlueprintPure, Category="PG|Ability")
	APGPlayerCharacter* GetPlayerCharacterFromActorInfo();

	/**
	 * 액터 정보로부터 플레이어 컨트롤러를 가져오는 함수
	 */
	UFUNCTION(BlueprintPure, Category="PG|Ability")
	APGPlayerController* GetPlayerControllerFromActorInfo();

	/**
	 * 액터 정보로부터 플레이어 컴뱃 컴포넌트를 가져오는 함수
	 */
	UFUNCTION(BlueprintPure, Category="PG|Ability")
	UPGPlayerCombatComponent* GetPlayerCombatComponentFromActorInfo();

	/**
	 * 플레이어 데미지 이펙트 스펙 핸들을 생성하는 함수
	 * @param EffectClass 적용할 이펙트 클래스
	 * @param InWeaponBaseDamage 무기 기본 데미지
	 * @param InCurrentAttackTypeTag 현재 공격 타입 태그
	 * @param InUsedComboCount 사용된 콤보 수
	 */
	UFUNCTION(BlueprintPure, Category = "PG|Ability")
	FGameplayEffectSpecHandle MakePlayerDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass,
		float InWeaponBaseDamage, FGameplayTag InCurrentAttackTypeTag, int32 InUsedComboCount);

public:
	
private:
	/**
	 * 캐시된 플레이어 캐릭터 참조
	 */
	TWeakObjectPtr<APGPlayerCharacter> CachedPlayerCharacter;
	
	/**
	 * 캐시된 플레이어 컨트롤러 참조
	 */
	TWeakObjectPtr<APGPlayerController> CachedPlayerController;
};
