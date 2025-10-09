#include "PGEnemyCombatComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/BoxComponent.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void UPGEnemyCombatComponent::OnHitTargetActor(AActor* HitActor)
{
	if (OverlappedActors.Contains(HitActor))
	{
		return;
	}

	OverlappedActors.AddUnique(HitActor);

	FGameplayEventData EventData;
	EventData.Instigator = GetOwningPawn();
	EventData.Target = HitActor;
	
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		HitActor,
		PGGamePlayTags::Shared_Event_HitReact,
		EventData);
}

void UPGEnemyCombatComponent::ToggleBodyCollisionBoxCollision(bool bShouldEnable, EPGToggleDamageType ToggleDamage)
{
	APGCharacterEnemy* OwningEnemyCharacter = GetOwningPawn<APGCharacterEnemy>();
	if (nullptr == OwningEnemyCharacter)
	{
		return;
	}
	
	switch (ToggleDamage)
	{
	case EPGToggleDamageType::LeftHand:
		if (UBoxComponent* LeftHandCollisionBox = OwningEnemyCharacter->GetLeftHandCollisionBox())
		{
			LeftHandCollisionBox->SetCollisionEnabled(bShouldEnable
			? ECollisionEnabled::QueryOnly
			: ECollisionEnabled::NoCollision);
		}
		break;
	case EPGToggleDamageType::RightHand:
		if (UBoxComponent* RightHandCollisionBox = OwningEnemyCharacter->GetRightHandCollisionBox())
		{
			RightHandCollisionBox->SetCollisionEnabled(bShouldEnable
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
		}
		break;
	case EPGToggleDamageType::LeftFoot:
		if (UBoxComponent* LeftFootCollisionBox = OwningEnemyCharacter->GetLeftFootCollisionBox())
		{
			LeftFootCollisionBox->SetCollisionEnabled(bShouldEnable
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
		}
		break;
	case EPGToggleDamageType::RightFoot:
		if (UBoxComponent* RightFootCollisionBox = OwningEnemyCharacter->GetRightFootCollisionBox())
		{
			RightFootCollisionBox->SetCollisionEnabled(bShouldEnable
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
		}
		break;
	case EPGToggleDamageType::Tail:
		if (UBoxComponent* TailCollisionBox = OwningEnemyCharacter->GetTailCollisionBox())
		{
			TailCollisionBox->SetCollisionEnabled(bShouldEnable
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
		}
		break;
	default:
		break;
	}

	if (false == bShouldEnable)
	{
		OverlappedActors.Empty();
	}
}
