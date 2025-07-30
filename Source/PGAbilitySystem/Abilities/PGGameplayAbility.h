

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "PGShared/Shared/Enum/PGEnumTypes.h"

#include "PGGameplayAbility.generated.h"

class UPGPawnCombatComponent;
class UPGAbilitySystemComponent;

/**
 * 어빌리티 활성화 정책을 정의하는 열거형
 */
UENUM(BlueprintType)
enum class EPGAbilityActivationPolicy : uint8
{
	OnTriggerd,		// 트리거 시 활성화
	OnGiven			// 부여 시 활성화
};
/**
 * 플레이그라운드 프로젝트의 기본 게임플레이 어빌리티 클래스
 * 모든 커스텀 어빌리티의 베이스 클래스로 사용
 */
UCLASS()
class PGABILITYSYSTEM_API UPGGameplayAbility : public UGameplayAbility
{ 
	GENERATED_BODY()

protected:
	/**
	 * 어빌리티 활성화 정책 설정
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	EPGAbilityActivationPolicy AbilityActivationPolicy = EPGAbilityActivationPolicy::OnTriggerd;

protected:
	UPROPERTY(Transient)
	FGameplayAbilitySpecHandle CachedSpecHandle;

	UPROPERTY(Transient)
	FGameplayAbilityActivationInfo CachedActivationInfo;

	const FGameplayAbilityActorInfo* CachedActorInfo;
	
public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	/**
	 * 어빌리티가 부여될 때 호출되는 함수
	 */
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/**
	 * 어빌리티가 종료될 때 호출되는 함수
	 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION()
	virtual void OnMontageCompleted();
	
protected:
	void EndAbilitySelf();
	
public:
	/**
	 * 액터 정보로부터 컴뱃 컴포넌트를 가져오는 함수
	 */
	UFUNCTION(BlueprintPure, Category="PG|Ability")
	UPGPawnCombatComponent* GetCombatComponentFromActorInfo() const;
	
	/**
	 * 액터 정보로부터 플레이어 컨트롤러를 가져오는 함수
	 */
	UFUNCTION(BlueprintPure, Category="PG|Ability")
	APGPlayerController* GetPlayerControllerFromActorInfo() const;
	
protected:
	/**
	 * 액터 정보로부터 PG 어빌리티 시스템 컴포넌트를 가져오는 함수
	 */
	UFUNCTION(BlueprintPure, Category="Ability")
	UPGAbilitySystemComponent* GetPGAbilitySystemComponentFromActorInfo() const;

	/**
	 * 대상 액터에게 게임플레이 이펙트 스펙 핸들을 적용하는 블루프린트 함수
	 */
	UFUNCTION(BlueprintCallable, Category="Ability", meta = (Displayname = "Apply Gameplay Effect Spec Handle To Target Actor", ExpandEnumAsExecs = "OutSuccessType"))
	FActiveGameplayEffectHandle BP_ApplyEffectSpecHandleToTarget(AActor* TargetActor, const FGameplayEffectSpecHandle& InSpecHandle, EPGSuccessType& OutSuccessType);
	
protected:
	/**
	 * 대상 액터에게 게임플레이 이펙트 스펙 핸들을 적용하는 네이티브 함수
	 */
	FActiveGameplayEffectHandle NativeApplyEffectSpecHandleToTarget(AActor* TargetActor, const FGameplayEffectSpecHandle& InSepcHandle);

};
