

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

public:
	/**
	 * 대상 액터를 히트했을 때 호출되는 오버라이드 함수
	 * @param HitActor 히트된 액터
	 */
	virtual void OnHitTargetActor(AActor* HitActor) override;
	
	/**
	 * 대상 액터에서 무기가 빠졌을 때 호출되는 오버라이드 함수
	 * @param InteractedActor 상호작용한 액터
	 */
	virtual void OnWeaponPulledFromTargetActor(AActor* InteractedActor) override;
	
public:
	/**
	 * 태그로 플레이어가 보유한 무기를 가져오는 함수
	 * @param InWeaponTag 무기 태그
	 * @return 플레이어 무기 객체
	 */
	UFUNCTION(BlueprintCallable, Category = "PG|Player|Combat")
	APGPlayerWeapon* GetPlayerCarriedWeaponByTag(FGameplayTag InWeaponTag) const;

	/**
	 * 플레이어가 현재 장착한 무기를 가져오는 함수
	 * @return 현재 장착된 플레이어 무기 객체
	 */
	UFUNCTION(BlueprintCallable, Category = "PG|Player|Combat")
	APGPlayerWeapon* GetPlayerCurrentEquippedWeapon() const;

	/**
	 * 플레이어가 현재 장착한 무기의 특정 레벨에서의 데미지를 가져오는 함수
	 * @param InLevel 무기 레벨
	 * @return 해당 레벨에서의 데미지 값
	 */
	UFUNCTION(BlueprintCallable, Category = "PG|Player|Combat")
	float GetPlayerCurrentEquippedWeaponDamageAtLevel(float InLevel) const;

};
