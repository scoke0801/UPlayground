


#include "PGAbilitySystemComponent.h"

#include "Abilities/PGPlayerGameplayAbility.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void UPGAbilitySystemComponent::OnAbilityInputPressed(const FGameplayTag& InInputTag)
{
	if (false == InInputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (false == AbilitySpec.DynamicAbilityTags.HasTagExact(InInputTag))
		{
			continue;
		}

		if (InInputTag.MatchesTag(PGGamePlayTags::InputTag_Toggleable))
		{
			if (AbilitySpec.IsActive())
			{
				CancelAbilityHandle(AbilitySpec.Handle);
			}
			else
			{
				TryActivateAbility(AbilitySpec.Handle);
			}
		}
		else
		{
			TryActivateAbility(AbilitySpec.Handle);
		}
	}
}

void UPGAbilitySystemComponent::OnAbilityInputReleased(const FGameplayTag& InInputTag)
{
	if (false == InInputTag.IsValid() ||
		false == InInputTag.MatchesTag(PGGamePlayTags::InputTag_MustBeHeld))
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.DynamicAbilityTags.HasTagExact(InInputTag) && AbilitySpec.IsActive())
		{
			CancelAbilityHandle(AbilitySpec.Handle);
		}
	}
}

void UPGAbilitySystemComponent::GrantPlayerWeaponAbilities(const TArray<FPGPlayerAbilitySet>& InDefaultWeaponAbilities,
	int32 ApplyLevel, TArray<FGameplayAbilitySpecHandle>& OutGrandesdAbilitySpecHandles)
{
	if (true == InDefaultWeaponAbilities.IsEmpty())
	{
		return;
	}

	for (const FPGPlayerAbilitySet& AbilitySet : InDefaultWeaponAbilities)
	{
		if (false == AbilitySet.IsValid())
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(AbilitySet.AbilityToGrant);
		AbilitySpec.SourceObject = GetAvatarActor();
		AbilitySpec.Level = ApplyLevel;
		AbilitySpec.DynamicAbilityTags.AddTag(AbilitySet.InputTag);
		
		OutGrandesdAbilitySpecHandles.AddUnique(GiveAbility(AbilitySpec));
	}
}

void UPGAbilitySystemComponent::RemoveGrantedPlayerAbilities(UPARAM(ref) TArray<FGameplayAbilitySpecHandle>& InSpecHandlesToRemove)
{
	if (true == InSpecHandlesToRemove.IsEmpty())
	{	
		return;
	}

	for (const FGameplayAbilitySpecHandle& AbilitySpec : InSpecHandlesToRemove)
	{
		if (AbilitySpec.IsValid())
		{
			ClearAbility(AbilitySpec);
		}
	}

	InSpecHandlesToRemove.Empty();
}

bool UPGAbilitySystemComponent::TryActivateAbilityByTag(FGameplayTag AbilityTagToActivate)
{
	check(AbilityTagToActivate.IsValid());

	TArray<FGameplayAbilitySpec*> FoundAbilitySpecs;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTagToActivate.GetSingleTagContainer(),
		FoundAbilitySpecs);

	if (false == FoundAbilitySpecs.IsEmpty())
	{
		const int32 RandomAbilityIndex = FMath::RandRange(0, FoundAbilitySpecs.Num() - 1);
		FGameplayAbilitySpec* SpecToActivate = FoundAbilitySpecs[RandomAbilityIndex];

		check(SpecToActivate);

		if (false == SpecToActivate->IsActive())
		{
			return TryActivateAbility(SpecToActivate->Handle);
		}
	}

	return false;
}
