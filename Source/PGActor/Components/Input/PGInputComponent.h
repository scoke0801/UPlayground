

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagContainer.h"
#include "PGData/DataAsset/Input/DataAsset_InputConfig.h"
#include "PGInputComponent.generated.h"

/**
 * 플레이그라운드 프로젝트의 커스텀 입력 컴포넌트 클래스
 * 게임플레이 태그 기반의 입력 바인딩과 어빌리티 입력 처리를 제공
 */
UCLASS()
class PGACTOR_API UPGInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	/**
	 * 네이티브 입력 액션을 바인딩하는 템플릿 함수
	 * @tparam UserObject 콜백 객체의 타입
	 * @tparam CallbackFunc 콜백 함수의 타입
	 * @param InInputConfig 입력 설정 데이터 에셋
	 * @param InInputTag 입력 태그
	 * @param TriggerEvent 트리거 이벤트 타입
	 * @param ContextObject 콜백 호출 대상 객체
	 * @param Func 콜백 함수
	 */
	template<class UserObject, typename CallbackFunc>
	void BindNativeInputAction(const UDataAsset_InputConfig* InInputConfig, const FGameplayTag& InInputTag,
		ETriggerEvent TriggerEvent, UserObject* ContextObject, CallbackFunc Func);

	/**
	 * 어빌리티 입력 액션을 바인딩하는 템플릿 함수
	 * @tparam UserObject 콜백 객체의 타입
	 * @tparam CallbackFunc 콜백 함수의 타입
	 * @param InInputConfig 입력 설정 데이터 에셋
	 * @param ContextObject 콜백 호출 대상 객체
	 * @param InputPressedFunc 입력 눌림 콜백 함수
	 * @param InputReleasedFunc 입력 해제 콜백 함수
	 */
	template<class UserObject, typename CallbackFunc>
	void BindAbilityInputAction(const UDataAsset_InputConfig* InInputConfig, UserObject* ContextObject,
		CallbackFunc InputPressedFunc, CallbackFunc InputReleasedFunc);


};

template <class UserObject, typename CallbackFunc>
void UPGInputComponent::BindNativeInputAction(const UDataAsset_InputConfig* InInputConfig,
	const FGameplayTag& InInputTag, ETriggerEvent TriggerEvent, UserObject* ContextObject, CallbackFunc Func)
{
	checkf(InInputConfig, TEXT("Input Config data Asset is null"));

	if (UInputAction* FoundAction = InInputConfig->FindNativeInputActionsByTag(InInputTag))
	{
		BindAction(FoundAction, TriggerEvent, ContextObject, Func);
	}
}

template <class UserObject, typename CallbackFunc>
void UPGInputComponent::BindAbilityInputAction(const UDataAsset_InputConfig* InInputConfig, UserObject* ContextObject,
	CallbackFunc InputPressedFunc, CallbackFunc InputReleasedFunc)
{
	checkf(InInputConfig, TEXT("Input Config data Asset is null"));

	for (const FPGInputActionConfig& AbilityInputActionConfig : InInputConfig->AbilityInputActions)
	{
		if (false == AbilityInputActionConfig.IsValid())
		{
			continue;
		}

		BindAction(AbilityInputActionConfig.InputAction, ETriggerEvent::Started, ContextObject, InputPressedFunc, AbilityInputActionConfig.InputTag);
		BindAction(AbilityInputActionConfig.InputAction, ETriggerEvent::Completed, ContextObject, InputReleasedFunc, AbilityInputActionConfig.InputTag);
	
	}
}
