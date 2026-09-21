


#include "PGDataAsset_EnemyStartUpData.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

void UPGDataAsset_EnemyStartUpData::GiveToAbilitySystemComponent(UAbilitySystemComponent* InASCToGive,
                                                                 int32 ApplyLevel)
{
	Super::GiveToAbilitySystemComponent(InASCToGive, ApplyLevel);

	if (false == EnemyCombatAbilities.IsEmpty())
	{
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : EnemyCombatAbilities)
		{
			if (nullptr == AbilityClass)
			{
				continue;
			}

			if (InASCToGive->FindAbilitySpecFromClass(AbilityClass)) continue;
            FGameplayAbilitySpec AbilitySpec(AbilityClass);
			AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
			AbilitySpec.Level = ApplyLevel;
			
			InASCToGive->GiveAbility(AbilitySpec);
		}
	}
}
