// Fill out your copyright notice in the Description page of Project Settings.


#include "PGPooledProjectile.h"

#include "PGProjectilePool.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "PGShared/Shared/Debug/PGDebugHelper.h"

void APGPooledProjectile::Fire(const FVector& StartLocation, const FVector& Direction, float Speed, float InDamage)
{
	// MovementComponent 재활성화
	if (MovementComponent)
	{
		MovementComponent->Activate();
		MovementComponent->SetUpdatedComponent(RootComponent);
	}

	FString Msg = FString::Printf(TEXT("This: %p, Loc (%f, %f, %f), Dir (%f, %f, %f)"), this,
		StartLocation.X, StartLocation.Y, StartLocation.Z,
		Direction.X, Direction.Y, Direction.Z);
	
	PG_Debug::Print(Msg);
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
	
	// MovementComponent 안전하게 정지
	if (MovementComponent)
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->Deactivate();
	}
	
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

