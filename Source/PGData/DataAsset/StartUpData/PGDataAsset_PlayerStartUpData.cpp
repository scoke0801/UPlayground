


#include "PGDataAsset_PlayerStartUpData.h"

#include "GameplayAbilitySpec.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

void UPGDataAsset_PlayerStartUpData::GiveToAbilitySystemComponent(UAbilitySystemComponent* InASCToGive,
	int32 ApplyLevel)
{
	Super::GiveToAbilitySystemComponent(InASCToGive, ApplyLevel);

	for (const FPGPlayerAbilitySet& AbilitySet : PlayerStartUpAbilitySets)
	{
		if (false == AbilitySet.IsValid())
		{
			continue;
		}
		if (InASCToGive->FindAbilitySpecFromClass(AbilitySet.AbilityToGrant)) continue;
        FGameplayAbilitySpec AbilitySpec(AbilitySet.AbilityToGrant);
		AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
		AbilitySpec.Level = ApplyLevel;
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilitySet.InputTag);
		
 		InASCToGive->GiveAbility(AbilitySpec);
	}
}
