


#include "DataAssets/StartUpData/PGDataAsset_EnemyStartUpData.h"

#include "AbilitySystem/PGAbilitySystemComponent.h"

void UPGDataAsset_EnemyStartUpData::GiveToAbilitySystemComponent(UPGAbilitySystemComponent* InASCToGive,
                                                                 int32 ApplyLevel)
{
	Super::GiveToAbilitySystemComponent(InASCToGive, ApplyLevel);

	if (false == EnemyCombatAbilities.IsEmpty())
	{
		for (const TSubclassOf<UPGEnemyGameplayAbility>& AbilityClass : EnemyCombatAbilities)
		{
			if (nullptr == AbilityClass)
			{
				continue;
			}

			FGameplayAbilitySpec AbilitySpec(AbilityClass);
			AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
			AbilitySpec.Level = ApplyLevel;
			
			InASCToGive->GiveAbility(AbilitySpec);
		}
	}
}
