


#include "PGEnemyGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"

APGCharacterEnemy* UPGEnemyGameplayAbility::GetEnemyCharacterFromActorInfo()
{
	if (false == CachedEmeneyCharacter.IsValid())
	{
		CachedEmeneyCharacter = Cast<APGCharacterEnemy>(CurrentActorInfo->AvatarActor);
	}

	return CachedEmeneyCharacter.IsValid() ? CachedEmeneyCharacter.Get() : nullptr;
}
