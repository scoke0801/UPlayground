


#include "PGDataAsset_PlayerStartUpData.h"

#include "GameplayAbilitySpec.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGAbilitySystem/Abilities/PGPlayerGameplayAbility.h"

void UPGDataAsset_PlayerStartUpData::GiveToAbilitySystemComponent(UPGAbilitySystemComponent* InASCToGive,
	int32 ApplyLevel)
{
	Super::GiveToAbilitySystemComponent(InASCToGive, ApplyLevel);

	for (const FPGPlayerAbilitySet& AbilitySet : PlayerStartUpAbilitySets)
	{
		if (false == AbilitySet.IsValid())
		{
			continue;
		}
		FGameplayAbilitySpec AbilitySpec(AbilitySet.AbilityToGrant);
		AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
		AbilitySpec.Level = ApplyLevel;
		AbilitySpec.DynamicAbilityTags.AddTag(AbilitySet.InputTag);
		
 		InASCToGive->GiveAbility(AbilitySpec);
	}
}
