


#include "PGPawnCombatComponent.h"

#include "GameplayTagContainer.h"
#include "Components/BoxComponent.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"

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

	InWeaponToRegister->OnWeaponHitTarget.BindUObject(this, &ThisClass::OnHitTargetActor);
	InWeaponToRegister->OnWeaponPullTarget.BindUObject(this, &ThisClass::OnWeaponPulledFromTargetActor);
	
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

void UPGPawnCombatComponent::SetCurrentEquippedWeaponTag(FGameplayTag WeaponTag)
{
	CurrentEquippedWeaponTag = WeaponTag;
}

void UPGPawnCombatComponent::ToggleWeaponCollision(bool bShouldEnable, EPGToggleDamageType ToggleDamageType)
{
	if (EPGToggleDamageType::CurrentEquippedWeapon == ToggleDamageType)
	{
		ToggleWeaponCollisionBoxCollision(bShouldEnable);
	}
	else
	{
		ToggleBodyCollisionBoxCollision(bShouldEnable, ToggleDamageType);
	}
}

void UPGPawnCombatComponent::OnHitTargetActor(AActor* HitActor)
{
}

void UPGPawnCombatComponent::OnWeaponPulledFromTargetActor(AActor* InteractedActor)
{
}

void UPGPawnCombatComponent::ToggleWeaponCollisionBoxCollision(bool bShouldEnable)
{
	APGWeaponBase* WeaponToToggle = GetCharacterCurrentEquippedWeapon();

	check(WeaponToToggle);

	if (bShouldEnable)
	{
		WeaponToToggle->GetWeaponCollisionBox()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		WeaponToToggle->GetWeaponCollisionBox()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OverlappedActors.Empty();
	}
}

void UPGPawnCombatComponent::ToggleBodyCollisionBoxCollision(bool bShouldEnable, EPGToggleDamageType ToggleDamage)
{
}
