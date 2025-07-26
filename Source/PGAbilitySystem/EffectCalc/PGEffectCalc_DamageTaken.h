

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "PGEffectCalc_DamageTaken.generated.h"

/**
 * 데미지 받기 계산을 담당하는 이펙트 계산 클래스
 * 공격력(AttackPower)과 방어력(DefensePower) 속성을 캡처하여 데미지 계산을 수행
 * 게임플레이 이펙트에서 복잡한 데미지 로직을 구현할 때 사용
 */
UCLASS()
class UPLAYGROUND_API UPGEffectCalc_DamageTaken : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()


public:
	/**
	 * 기본 생성자
	 * 공격력, 방어력, 데미지받기 속성을 캡처하도록 설정
	 */
	UPGEffectCalc_DamageTaken();

	/**
	 * 데미지 계산을 실제로 수행하는 가상 함수 구현
	 * 
	 * 계산 과정:
	 * 1. 공격자의 공격력과 피격자의 방어력을 캡처
	 * 2. SetByCaller로 설정된 기본 데미지와 콤보 카운트를 추출
	 * 3. 콤보 시스템에 따른 데미지 증가율 적용
	 * 4. 최종 데미지 = 기본데미지 * 콤보배율 * (공격력 / 방어력)
	 * 5. 계산된 데미지를 DamageTaken 속성에 적용
	 * 
	 * @param ExecutionParams 실행 매개변수 (이펙트 스펙, 캡처된 속성 등 포함)
	 * @param OutExecutionOutput 출력 결과 (수정할 속성과 값을 담는 구조체)
	 */
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
	
};
