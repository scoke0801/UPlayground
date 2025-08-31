// Fill out your copyright notice in the Description page of Project Settings.


#include "PGPooledProjectile.h"

#include "PGProjectilePool.h"
#include "GameFramework/ProjectileMovementComponent.h"

void APGPooledProjectile::Fire(const FVector& StartLocation, const FVector& Direction, float Speed, float InDamage)
{
	Super::Fire(StartLocation, Direction, Speed, InDamage);
	
	bInUse = true;
}

void APGPooledProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::OnProjectileHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
}

void APGPooledProjectile::OnLifeTimeExpired()
{
	ReturnToPool();
}

void APGPooledProjectile::ReturnToPool()
{
	if (!bInUse) return;

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	MovementComponent->StopMovementImmediately();
	bInUse = false;

	GetWorldTimerManager().ClearTimer(LifeTimeHandle);

	if (OwnerPool)
	{
		OwnerPool->ReturnProjectile(this);
	}
}

