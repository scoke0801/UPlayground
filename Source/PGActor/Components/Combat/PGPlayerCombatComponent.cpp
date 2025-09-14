


#include "PGPlayerCombatComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "PGActor/Weapon/PGPlayerWeapon.h"
#include "PGShared/Shared/Debug/PGDebugHelper.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void UPGPlayerCombatComponent::OnHitTargetActor(AActor* HitActor)
{
	// 여러 번 피격해도 괜찮게 보여서 우선 주석처리
	//if (OverlappedActors.Contains(HitActor))
	//{
	//	return;
	//}

	OverlappedActors.AddUnique(HitActor);

	FGameplayEventData Data;
	Data.Instigator = GetOwningPawn();
	Data.Target = HitActor;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwningPawn(),
		PGGamePlayTags::Shared_Event_Hit,
		Data
		);

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwningPawn(),
		PGGamePlayTags::Player_Event_HitPause,
		FGameplayEventData()
	);
}

void UPGPlayerCombatComponent::OnWeaponPulledFromTargetActor(AActor* InteractedActor)
{
	// FGameplayEventData Data;
	// Data.Instigator = GetOwningPawn();
	// Data.Target = InteractedActor;
	//
	// UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
	// 	GetOwningPawn(),
	// 	PGGamePlayTags::Shared_Event_Hit,
	// 	Data
	// 	);

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwningPawn(),
		PGGamePlayTags::Player_Event_HitPause,
		FGameplayEventData()
	);
}

APGPlayerWeapon* UPGPlayerCombatComponent::GetPlayerCarriedWeaponByTag(FGameplayTag InWeaponTag) const
{
	return Cast<APGPlayerWeapon>(GetCharacterCarriedWeaponByTag(InWeaponTag));
}

APGPlayerWeapon* UPGPlayerCombatComponent::GetPlayerCurrentEquippedWeapon() const
{
	return Cast<APGPlayerWeapon>(GetCharacterCurrentEquippedWeapon());
}

float UPGPlayerCombatComponent::GetPlayerCurrentEquippedWeaponDamageAtLevel(float InLevel) const
{
	return GetPlayerCurrentEquippedWeapon()->WeaponData.WeaponBaseDamage.GetValueAtLevel(InLevel);
}
