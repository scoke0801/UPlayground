


#include "PGDataAsset_StartUpDataBase.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

void UPGDataAsset_StartUpDataBase::GiveToAbilitySystemComponent(UAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
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

void UPGDataAsset_StartUpDataBase::GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& InAbilitiesToGive,
	UAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
	if (InAbilitiesToGive.IsEmpty())
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& Ability : InAbilitiesToGive)
	{
		if (nullptr == Ability)
		{
			continue;
		}

		if (InASCToGive->FindAbilitySpecFromClass(Ability)) continue;
        FGameplayAbilitySpec AbilitySpec(Ability);
		AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
		AbilitySpec.Level = ApplyLevel;
		
		InASCToGive->GiveAbility(AbilitySpec);
	}
}
