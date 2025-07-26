


#include "AbilitySystem/Abilities/PGEnemyGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "PGGamePlayTags.h"
#include "Characters/NonPlayer/PGEnemyCharacter.h"

APGEnemyCharacter* UPGEnemyGameplayAbility::GetEnemyCharacterFromActorInfo()
{
	if (false == CachedEmeneyCharacter.IsValid())
	{
		CachedEmeneyCharacter = Cast<APGEnemyCharacter>(CurrentActorInfo->AvatarActor);
	}

	return CachedEmeneyCharacter.IsValid() ? CachedEmeneyCharacter.Get() : nullptr;
}

UPGEnemyCombatComponent* UPGEnemyGameplayAbility::GetEnemeyCombatComponentFromActorInfo()
{
	if (false == CachedEmeneyCharacter.IsValid())
	{
		return nullptr;
	}
	
	return CachedEmeneyCharacter->GetEnemyCombatComponent();
}

FGameplayEffectSpecHandle UPGEnemyGameplayAbility::MakeEnemyDamageEffectSpecHandle(
	TSubclassOf<UGameplayEffect> EffectClass, const FScalableFloat& InDamageScalableFloat)
{
	check(EffectClass);

	// 게임플레이 이펙트 컨텍스트 핸들 생성
	// 이펙트의 소스, 대상, 발생 위치 등의 정보를 담는 컨텍스트
	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
	
	// 컨텍스트에 현재 어빌리티를 설정 (이펙트를 발생시킨 어빌리티 추적용)
	ContextHandle.SetAbility(this);
	
	// 컨텍스트에 소스 오브젝트 추가 (이펙트를 발생시킨 액터)
	ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());
	
	// 컨텍스트에 발동자(Instigator) 추가 (이펙트를 실제로 발생시킨 액터와 원인 액터)
	ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

	// 게임플레이 이펙트 스펙 핸들 생성
	// 실제 이펙트 인스턴스를 만들기 위한 명세서 역할
	FGameplayEffectSpecHandle EffectSpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(
		EffectClass,			// 사용할 이펙트 클래스
		GetAbilityLevel(),		// 어빌리티 레벨
		ContextHandle			// 앞서 생성한 컨텍스트
		);
	
	// SetByCaller 방식으로 기본 데미지 값 설정
	// 블루프린트나 다른 곳에서 동적으로 값을 설정할 수 있도록 태그 기반으로 설정
	EffectSpecHandle.Data->SetSetByCallerMagnitude(
		PGGamePlayTags::Shared_SetByCaller_BaseDamage,
		InDamageScalableFloat.GetValueAtLevel(GetAbilityLevel()));

	return EffectSpecHandle;
}
