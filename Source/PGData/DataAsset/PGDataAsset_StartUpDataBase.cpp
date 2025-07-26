


#include "DataAssets/StartUpData/PGDataAsset_StartUpDataBase.h"

#include "AbilitySystem/PGAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/PGGameplayAbility.h"

void UPGDataAsset_StartUpDataBase::GiveToAbilitySystemComponent(UPGAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
	check(InASCToGive);
	
	GrantAbilities(ActivateOnGivenAbilities, InASCToGive, ApplyLevel);

	GrantAbilities(ReactiveAbilities, InASCToGive,  ApplyLevel);

	if (false == StartUpGameplayEffects.IsEmpty())
	{
		for (const TSubclassOf<UGameplayEffect>& EffectClass : StartUpGameplayEffects)
		{
			if (nullptr == EffectClass)
			{
				continue;
			}

			UGameplayEffect* EffectCDO = EffectClass->GetDefaultObject<UGameplayEffect>();
			InASCToGive->ApplyGameplayEffectToSelf(
				EffectCDO,
				ApplyLevel,
				InASCToGive->MakeEffectContext());
		}
	}
}

void UPGDataAsset_StartUpDataBase::GrantAbilities(const TArray<TSubclassOf<UPGGameplayAbility>>& InAbilitiesToGive,
	UPGAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
	if (InAbilitiesToGive.IsEmpty())
	{
		return;
	}

	for (const TSubclassOf<UPGGameplayAbility>& Ability : InAbilitiesToGive)
	{
		if (nullptr == Ability)
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(Ability);
		AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
		AbilitySpec.Level = ApplyLevel;
		
		InASCToGive->GiveAbility(AbilitySpec);
	}
}
