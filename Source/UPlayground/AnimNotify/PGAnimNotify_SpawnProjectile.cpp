// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAnimNotify_SpawnProjectile.h"

#include "PGActor/Projectile/Pool/PGProjectileManager.h"

void UPGAnimNotify_SpawnProjectile::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                           const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* OwnerActor = MeshComp->GetOwner();
	if (nullptr == OwnerActor)
	{
		return;
	}

	if (UPGProjectileManager* Manager = PGProjectile())
	{
		Manager->FireProjectile(ProjectileId,
		OwnerActor->GetActorLocation(),
		OwnerActor->GetActorForwardVector());
	}
}
