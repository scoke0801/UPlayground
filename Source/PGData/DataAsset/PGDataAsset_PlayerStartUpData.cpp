


#include "DataAssets/StartUpData/PGDataAsset_PlayerStartUpData.h"

#include "AbilitySystem/PGAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/PGGameplayAbility.h"
#include "Types/PlayerStructTypes.h"

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
