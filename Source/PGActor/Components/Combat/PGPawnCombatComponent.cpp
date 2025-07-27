


#include "PGPawnCombatComponent.h"

#include "GameplayTagContainer.h"

APGWeaponBase* UPGPawnCombatComponent::GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const
{
	if (CharacterCarriedWeaponMap.Contains(InWeaponTagToGet))
	{
		if (APGWeaponBase* const* Weapon = CharacterCarriedWeaponMap.Find(InWeaponTagToGet))
		{
			return *Weapon;
		}
	}

	return nullptr;
}

void UPGPawnCombatComponent::RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister,
	APGWeaponBase* InWeaponToRegister, bool bRegsisterAsEquippedWeapon)
{
	CharacterCarriedWeaponMap.Emplace(InWeaponTagToRegister, InWeaponToRegister);

	//InWeaponToRegister->OnWeaponHitTarget.BindUObject(this, &ThisClass::OnHitTargetActor);
	//InWeaponToRegister->OnWeaponPullTarget.BindUObject(this, &ThisClass::OnWeaponPulledFromTargetActor);
	
	if (bRegsisterAsEquippedWeapon)
	{
		CurrentEquippedWeaponTag = InWeaponTagToRegister;
	}
}

APGWeaponBase* UPGPawnCombatComponent::GetCharacterCurrentEquippedWeapon() const
{
	if (false == CurrentEquippedWeaponTag.IsValid())
	{
		return nullptr;
	}

	return GetCharacterCarriedWeaponByTag(CurrentEquippedWeaponTag);
}

void UPGPawnCombatComponent::SetCurrentEquippWeaponTag(FGameplayTag WeaponTag)
{
	CurrentEquippedWeaponTag = WeaponTag;
}
