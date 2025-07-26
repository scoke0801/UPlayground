

#include "AbilitySystem/Abilities/PGPlayerGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "PGGamePlayTags.h"
#include "Characters/Player/PGPlayerCharacter.h"
#include "Controllers/PGPlayerController.h"

APGPlayerCharacter* UPGPlayerGameplayAbility::GetPlayerCharacterFromActorInfo()
{
	// 캐시된 플레이어 캐릭터가 유효하지 않은 경우에만 새로 캐스팅
	// 성능 최적화를 위해 한 번 캐스팅한 결과를 재사용
	if (false == CachedPlayerCharacter.IsValid())
	{
		// CurrentActorInfo의 AvatarActor를 APGPlayerCharacter로 캐스팅하여 캐시
		CachedPlayerCharacter = Cast<APGPlayerCharacter>(CurrentActorInfo->AvatarActor);
	}
	
	// 캐시된 플레이어 캐릭터가 유효하면 반환, 아니면 nullptr 반환
	return CachedPlayerCharacter.IsValid() ? CachedPlayerCharacter.Get() : nullptr;
}

APGPlayerController* UPGPlayerGameplayAbility::GetPlayerControllerFromActorInfo()
{
	// 캐시된 플레이어 컨트롤러가 유효하지 않은 경우에만 새로 캐스팅
	// 성능 최적화를 위해 한 번 캐스팅한 결과를 재사용
	if (false == CachedPlayerController.IsValid())
	{
		// CurrentActorInfo의 PlayerController를 APGPlayerController로 캐스팅하여 캐시
		CachedPlayerController = Cast<APGPlayerController>(CurrentActorInfo->PlayerController);
	}
	
	// 캐시된 플레이어 컨트롤러가 유효하면 반환, 아니면 nullptr 반환
	return CachedPlayerController.IsValid() ? CachedPlayerController.Get() : nullptr;
}

UPGPlayerCombatComponent* UPGPlayerGameplayAbility::GetPlayerCombatComponentFromActorInfo()
{
	// 플레이어 캐릭터로부터 직접 플레이어 컴뱃 컴포넌트를 가져와 반환
	// GetPlayerCharacterFromActorInfo()를 통해 캐시된 플레이어 캐릭터를 사용
	return GetPlayerCharacterFromActorInfo()->GetPlayerCombatComponent();
}

FGameplayEffectSpecHandle UPGPlayerGameplayAbility::MakePlayerDamageEffectSpecHandle(
	TSubclassOf<UGameplayEffect> EffectClass, float InWeaponBaseDamage, FGameplayTag InCurrentAttackTypeTag,
	int32 InUsedComboCount)
{
	// 이펙트 클래스가 유효한지 확인 (디버그 빌드에서 크래시 발생)
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
		PGGamePlayTags::Shared_SetByCaller_BaseDamage,	// 기본 데미지 태그
		InWeaponBaseDamage									// 무기 기본 데미지 값
	);

	// 현재 공격 타입 태그가 유효한 경우 콤보 카운트 설정
	// 콤보 시스템에서 공격 타입별로 다른 데미지 계수를 적용하기 위함
	if (InCurrentAttackTypeTag.IsValid())
	{
		EffectSpecHandle.Data->SetSetByCallerMagnitude(
			InCurrentAttackTypeTag,		// 공격 타입 태그
			InUsedComboCount			// 사용된 콤보 수
		);
	}
	
	// 완성된 이펙트 스펙 핸들 반환
	return EffectSpecHandle;
}
