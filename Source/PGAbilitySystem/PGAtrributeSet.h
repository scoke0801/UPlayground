

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "PGAtrributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

class IPGPawnUIInterface;
/**
 * 플레이그라운드 프로젝트의 어트리뷰트 셋 클래스
 * 캐릭터의 체력, 분노, 공격력, 방어력 등의 속성을 관리
 */
UCLASS()
class UPLAYGROUND_API UPGAtrributeSet : public UAttributeSet
{
	GENERATED_BODY()


public:
	/**
	 * 기본 생성자
	 */
	UPGAtrributeSet();

public:
	/**
	 * 게임플레이 이펙트 실행 후 호출되는 함수
	 * 어트리뷰트 값의 유효성 검증 및 클램핑 처리를 수행
	 */
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
	
public:
	/**
	 * 현재 체력 값
	 */
	UPROPERTY(BlueprintReadOnly, Category = "PG|Health")
	FGameplayAttributeData CurrentHealth;
	ATTRIBUTE_ACCESSORS(UPGAtrributeSet, CurrentHealth)

	/**
	 * 최대 체력 값
	 */
	UPROPERTY(BlueprintReadOnly, Category = "PG|Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UPGAtrributeSet, MaxHealth)

	/**
	 * 현재 분노 값
	 */
	UPROPERTY(BlueprintReadOnly, Category = "PG|Rage")
	FGameplayAttributeData CurrentRage;
	ATTRIBUTE_ACCESSORS(UPGAtrributeSet, CurrentRage)

	/**
	 * 최대 분노 값
	 */
	UPROPERTY(BlueprintReadOnly, Category = "PG|Rage")
	FGameplayAttributeData MaxRage;
	ATTRIBUTE_ACCESSORS(UPGAtrributeSet, MaxRage)

	/**
	 * 공격력 값
	 */
	UPROPERTY(BlueprintReadOnly, Category = "PG|Damage")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UPGAtrributeSet, AttackPower)

	/**
	 * 방어력 값
	 */
	UPROPERTY(BlueprintReadOnly, Category = "PG|Damage")
	FGameplayAttributeData DefensePower;
	ATTRIBUTE_ACCESSORS(UPGAtrributeSet, DefensePower)

	/**
	 * 받은 데미지 값 (임시 계산용 어트리뷰트)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "PG|Damage")
	FGameplayAttributeData DamageTaken;
	ATTRIBUTE_ACCESSORS(UPGAtrributeSet, DamageTaken)

private:
	TWeakInterfacePtr<IPGPawnUIInterface> CachedPawnUIInterface;
};
