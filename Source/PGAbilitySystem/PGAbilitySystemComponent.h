

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "PGShared/Shared/Structure/PlayerStructTypes.h"
#include "PGAbilitySystemComponent.generated.h"

/**
 * 플레이그라운드 프로젝트의 커스텀 어빌리티 시스템 컴포넌트
 * 플레이어의 입력 처리와 무기 어빌리티 관리를 담당
 */
UCLASS()
class PGABILITYSYSTEM_API UPGAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()


public:
	/**
	 * 어빌리티 입력이 눌렸을 때 호출되는 함수
	 * @param InInputTag 입력된 게임플레이 태그
	 */
	void OnAbilityInputPressed(const FGameplayTag& InInputTag);
	
	/**
	 * 어빌리티 입력이 해제되었을 때 호출되는 함수
	 * @param InInputTag 해제된 게임플레이 태그
	 */
	void OnAbilityInputReleased(const FGameplayTag& InInputTag);

	/**
	 * 플레이어 무기 어빌리티들을 부여하는 함수
	 * @param InDefaultWeaponAbilities 부여할 기본 무기 어빌리티 배열
	 * @param ApplyLevel 적용할 레벨
	 * @param OutGrandesdAbilitySpecHandles 부여된 어빌리티 스펙 핸들 배열 (출력)
	 */
	UFUNCTION(BlueprintCallable, Category = "PG|Ability", meta = (ApplyLevel ="1"))
	void GrantPlayerWeaponAbilities(const TArray<FPGPlayerAbilitySet>& InDefaultWeaponAbilities, int32 ApplyLevel,
		TArray<FGameplayAbilitySpecHandle>& OutGrandesdAbilitySpecHandles);

	/**
	 * 부여된 플레이어 어빌리티들을 제거하는 함수
	 * @param InSpecHandlesToRemove 제거할 어빌리티 스펙 핸들 배열
	 */
	UFUNCTION(BlueprintCallable, Category = "PG|Ability", meta = (ApplyLevel ="1"))
	void RemoveGrantedPlayerAbilities(UPARAM(ref) TArray<FGameplayAbilitySpecHandle>& InSpecHandlesToRemove);

	UFUNCTION(BlueprintCallable, Category = "PG|Ability")
	bool TryActivateAbilityByTag(FGameplayTag AbilityTagToActivate);	
};
