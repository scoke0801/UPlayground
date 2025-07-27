


#include "PGAtrributeSet.h"

#include "GameplayEffectExtension.h"

/**
 * 기본 생성자
 * 모든 어트리뷰트를 기본값 1.0으로 초기화
 */
UPGAtrributeSet::UPGAtrributeSet()
{
	InitCurrentHealth(1.f);
	InitMaxHealth(1.f);
	InitCurrentRage(1.f);
	InitMaxRage(1.f);
	InitAttackPower(1.f);
	InitDefensePower(1.f);
}

/**
 * 게임플레이 이펙트 실행 후 호출되는 함수
 * 어트리뷰트 값의 유효성 검증 및 클램핑 처리를 수행
 */
void UPGAtrributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{

}
