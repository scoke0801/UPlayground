// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/PGAnimNotify_LandingFromJump.h"

#include "AnimInstances/PGCharacterAnimInstance.h"
#include "Helper/Debug/PGDebugHelper.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "Slate/SGameLayerManager.h"


void UPGAnimNotify_LandingFromJump::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                           const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(MeshComp->GetOwner()))
	{
		Player->bIsJump = false;
	}
	else
	{
		PG_Debug::Print(TEXT("Fail Land"));
	}

	PG_Debug::Print(TEXT("JumpLanded"));
}
