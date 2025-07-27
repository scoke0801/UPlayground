

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PGPawnExtensionComponentBase.generated.h"


/**
 * 플레이그라운드 프로젝트의 폰 확장 컴포넌트 베이스 클래스
 * 폰과 관련된 공통 기능을 제공하는 액터 컴포넌트의 기본 클래스
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PGACTOR_API UPGPawnExtensionComponentBase : public UActorComponent
{
	GENERATED_BODY()

protected:
	/**
	 * 지정된 타입으로 소유하는 폰을 가져오는 템플릿 함수
	 * @tparam T 폰의 타입 (APawn에서 파생되어야 함)
	 * @return 타입 캐스팅된 소유 폰
	 */
	template <class T>
	T* GetOwningPawn() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, APawn>::Value, "'T' Template Parameter get GetPawn() must be derived from APawn.");
		return CastChecked<T>(GetOwner());
	}

	/**
	 * 소유하는 폰을 APawn 타입으로 가져오는 함수
	 * @return 소유 폰
	 */
	APawn* GetOwningPawn() const
	{
		return GetOwningPawn<APawn>();
	}
	
	/**
	 * 지정된 타입으로 소유하는 컨트롤러를 가져오는 템플릿 함수
	 * @tparam T 컨트롤러의 타입 (AController에서 파생되어야 함)
	 * @return 타입 캐스팅된 소유 컨트롤러
	 */
	template <class T>
	T* GetOwningController() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, AController>::Value, "'T' Template Parameter get GetController() must be derived from AController.");
		return GetOwningPawn<APawn>()->GetController<T>();
	}
};
