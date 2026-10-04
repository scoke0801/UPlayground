


#include "PGPawnCombatComponent.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGPlayerAttackComponent.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"

#include "GameplayTagContainer.h"
#include "Components/BoxComponent.h"
#include "PGActor/Weapon/PGWeaponBase.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"

void UPGPawnCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Stage cleanup/respawn can destroy the pawn without its death/dissolve path.
    // Registered weapons belong to this component for the pawn's lifetime.
    for (const auto& Pair : CharacterCarriedWeaponMap)
        if (IsValid(Pair.Value)) Pair.Value->Destroy();
    CharacterCarriedWeaponMap.Empty();
    CurrentEquippedWeaponTag = FGameplayTag();
    OverlappedActors.Empty();
    Super::EndPlay(EndPlayReason);
}

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
		SetCurrentEquippedWeaponTag(InWeaponTagToRegister);
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
    if (APGCharacterBase* Character = Cast<APGCharacterBase>(GetOwner()))
    {
        TMap<EPGStatType, int32> Bonuses;
        if (const APGWeaponBase* Weapon = GetCharacterCurrentEquippedWeapon())
            for (uint8 Index = 1; Index < static_cast<uint8>(EPGStatType::Max); ++Index)
            {
                const auto Type = static_cast<EPGStatType>(Index);
                const int32 Value = Weapon->GetWeaponStat(Type);
                if (Value != 0) Bonuses.Add(Type, Value);
            }
        Character->GetPGAbilitySystemComponent()->SetEquipmentBonuses(Bonuses);
    }
}

void UPGPawnCombatComponent::ToggleWeaponCollision(bool bShouldEnable, EPGToggleDamageType ToggleDamageType)
{
    if (const auto* Player = Cast<APGCharacterPlayer>(GetOwner()))
        if (Player->GetPlayerAttackComponent()->IsRunning()) bShouldEnable = false;
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

	// NotifyEnd may arrive after EndPlay/unequip has already destroyed the weapon.
    if (!IsValid(WeaponToToggle) || !IsValid(WeaponToToggle->GetWeaponCollisionBox()))
    {
        OverlappedActors.Empty();
        return;
    }

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
