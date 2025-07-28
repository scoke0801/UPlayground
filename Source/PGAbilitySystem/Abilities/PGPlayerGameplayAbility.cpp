

#include "PGPlayerGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Controllers/PGPlayerController.h"

APGCharacterPlayer* UPGPlayerGameplayAbility::GetPlayerCharacterFromActorInfo()
{
	// 캐시된 플레이어 캐릭터가 유효하지 않은 경우에만 새로 캐스팅
	// 성능 최적화를 위해 한 번 캐스팅한 결과를 재사용
	if (false == CachedPlayerCharacter.IsValid())
	{
		// CurrentActorInfo의 AvatarActor를 APGPlayerCharacter로 캐스팅하여 캐시
		CachedPlayerCharacter = Cast<APGCharacterPlayer>(CurrentActorInfo->AvatarActor);
	}
	
	// 캐시된 플레이어 캐릭터가 유효하면 반환, 아니면 nullptr 반환
	return CachedPlayerCharacter.IsValid() ? CachedPlayerCharacter.Get() : nullptr;
}

UPGPlayerCombatComponent* UPGPlayerGameplayAbility::GetPlayerCombatComponentFromActorInfo()
{
	// 플레이어 캐릭터로부터 직접 플레이어 컴뱃 컴포넌트를 가져와 반환
	// GetPlayerCharacterFromActorInfo()를 통해 캐시된 플레이어 캐릭터를 사용
	return GetPlayerCharacterFromActorInfo()->GetPlayerCombatComponent();
}

