


#include "PGAbilitySystemComponent.h"

#include "Abilities/PGPlayerGameplayAbility.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataTemplate.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void UPGAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	DelegateHandle = UPGMessageManager::Get()->RegisterDelegate(EPGUIMessageType::ClickSkillButton, this,
		&ThisClass::OnClickedSkillButton);
}

void UPGAbilitySystemComponent::BeginDestroy()
{
	if (DelegateHandle.IsValid())
	{
		if (auto Manager = UPGMessageManager::Get())
		{
			Manager->UnregisterDelegate(EPGUIMessageType::ClickSkillButton, DelegateHandle);
		}
	}
	
	Super::BeginDestroy();
}

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
			// Ability 에서 Tag정보 사용할 수 있도록 임시 설정
			SpecToActivate->DynamicAbilityTags.AddTag(AbilityTagToActivate);
			
			bool bResult = TryActivateAbility(SpecToActivate->Handle);

			// 활성화 후 태그 제거
			SpecToActivate->DynamicAbilityTags.RemoveTag(AbilityTagToActivate);

			return bResult;
		}
	}

	return false;
}

void UPGAbilitySystemComponent::OnClickedSkillButton(const IPGEventData* InData)
{
	const FPGEventDataOneParam<FGameplayTag>* CastedParam =
		static_cast<const FPGEventDataOneParam<FGameplayTag>*>(InData);

	if (nullptr == CastedParam)
	{
		return;
	}
	OnAbilityInputPressed(CastedParam->Value);
}
