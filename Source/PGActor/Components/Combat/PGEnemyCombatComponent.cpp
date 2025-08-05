


#include "PGEnemyCombatComponent.h"

void UPGEnemyCombatComponent::OnHitTargetActor(AActor* HitActor)
{
	if (OverlappedActors.Contains(HitActor))
	{
		return;
	}

	OverlappedActors.AddUnique(HitActor);

	// TODO:: Block Check 구현
	bool bIsValidBlock = false;

	// const bool bIsPlayerBlocking = UPGFunctionLibrary::NativeDoesActorHaveTag(HitActor, PGGamePlayTags::Player_Status_Blocking);
	// const bool bIsMyAttackUnblockable = false;
	//
	// FGameplayEventData EventData;
	// EventData.Instigator = GetOwningPawn();
	// EventData.Target = HitActor;
	//
	// if (bIsPlayerBlocking && false == bIsMyAttackUnblockable)
	// {
	// 	bIsValidBlock = UPGFunctionLibrary::IsValidBlock(GetOwningPawn(), HitActor);
	// }
	//
	// if (bIsValidBlock)
	// {
	// 	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
	// 		HitActor,
	// 		PGGamePlayTags::Player_Event_Block_Attack,
	// 		EventData);
	// }
	// else
	// {
	// 	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
	// 		GetOwningPawn(),
	// 		PGGamePlayTags::Shared_Event_MeleeHit,
	// 		EventData);
	// }
}
