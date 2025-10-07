// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAnimNotify_SpawnProjectile.h"

#include "GenericTeamAgentInterface.h"
#include "PGActor/AreaOfEffect/PGAreaOfEffectBase.h"
#include "PGActor/Projectile/Pool/PGProjectileManager.h"

void UPGAnimNotify_SpawnProjectile::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                           const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner =MeshComp->GetOwner();
	if (nullptr == Owner)
	{
		return;
	}
	
	if (UPGProjectileManager* Manager = PGProjectile())
	{
		Manager->FireProjectile(ProjectileId,
		Owner,
		Owner->GetActorLocation(),
		Owner->GetActorForwardVector());
	}

	// Test Code
	APGAreaOfEffectBase::Fire(Owner, Owner->GetActorLocation(), 1, 0.0f);
}
