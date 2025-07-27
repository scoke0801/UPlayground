

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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

protected:
	/**
	 * 현재 장착된 무기의 태그
	 */
	UPROPERTY(BlueprintReadWrite, Category="Combat")
	FGameplayTag CurrentEquippedWeaponTag;

protected:
	/**
	 * 겹쳐진 액터들의 배열
	 */
	UPROPERTY(Transient)
	TArray<AActor*> OverlappedActors;
	
private:
	/**
	 * 캐릭터가 보유한 무기들의 맵 (태그 -> 무기)
	 */
	UPROPERTY(Transient)
	TMap<FGameplayTag, APGWeaponBase*> CharacterCarriedWeaponMap;
	
public:
	/**
	 * 태그로 캐릭터가 보유한 무기를 가져오는 함수
	 * @param InWeaponTagToGet 가져올 무기의 태그
	 * @return 해당 태그의 무기 객체
	 */
	UFUNCTION(BlueprintCallable, Category = "PG|Combat")
	APGWeaponBase* GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const;

	/**
	 * 생성된 무기를 등록하는 함수
	 * @param InWeaponTagToRegister 등록할 무기의 태그
	 * @param InWeaponToRegister 등록할 무기 객체
	 * @param bRegsisterAsEquippedWeapon 장착된 무기로 등록할지 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "PG|Combat")
	void RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, APGWeaponBase* InWeaponToRegister, bool bRegsisterAsEquippedWeapon = false);
	
	/**
	 * 캐릭터가 현재 장착한 무기를 가져오는 함수
	 * @return 현재 장착된 무기 객체
	 */
	UFUNCTION(BlueprintCallable, Category = "PG|Combat")
	APGWeaponBase* GetCharacterCurrentEquippedWeapon() const;
	
	UFUNCTION(BlueprintCallable, Category = "PG|Combat")
	void SetCurrentEquippWeaponTag(FGameplayTag WeaponTag);
};
