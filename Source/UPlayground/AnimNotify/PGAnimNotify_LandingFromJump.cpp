// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/PGAnimNotify_LandingFromJump.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"


void UPGAnimNotify_LandingFromJump::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                           const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(MeshComp->GetOwner()))
	{
		FGameplayEventData Data;
		Data.EventTag = PGGamePlayTags::Player_Event_JumpLanded;
		Data.Instigator = Player;
		Data.Target = Player;
		
 		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Player,
			PGGamePlayTags::Player_Event_JumpLanded,
			Data
		);

		// Ability 에 의한 활성화가 아니라, 단순 Falling 처리에 의한 것일수도 있으니 false!
		Player->SetIsJump(false);
	}
}
