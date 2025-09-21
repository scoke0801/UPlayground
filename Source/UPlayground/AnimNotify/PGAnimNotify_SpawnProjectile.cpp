// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAnimNotify_SpawnProjectile.h"

#include "GenericTeamAgentInterface.h"
#include "PGActor/Projectile/Pool/PGProjectileManager.h"

void UPGAnimNotify_SpawnProjectile::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                           const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	APawn* OwnerPawn = Cast<APawn>(MeshComp->GetOwner());
	if (nullptr == OwnerPawn)
	{
		return;
	}
	
	IGenericTeamAgentInterface* QueryTeamAgent = Cast<IGenericTeamAgentInterface>(OwnerPawn->GetController());
	if (nullptr == QueryTeamAgent)
	{
		return;
	}

	if (UPGProjectileManager* Manager = PGProjectile())
	{
		Manager->FireProjectile(ProjectileId,
			QueryTeamAgent->GetGenericTeamId(),
		OwnerPawn->GetActorLocation(),
		OwnerPawn->GetActorForwardVector());
	}
}
