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

	Super::Fire(StartLocation, Direction, Speed, InDamage);
	
	bInUse = true;
}

void APGPooledProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::OnProjectileHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
	
	// 히트 후 풀로 반환
	//ReturnToPool();
}

void APGPooledProjectile::OnLifeTimeExpired()
{
	// 수명 만료 시 풀로 반환
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
	
	// 투사체 비활성화
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	bInUse = false;

	// 타이머 정리
	GetWorldTimerManager().ClearTimer(LifeTimeHandle);

	// Pool이 유효한지 확인 후 반환
	if (IsValid(OwnerPool))
	{
		OwnerPool->ReturnProjectile(this);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ReturnToPool - OwnerPool is invalid"));
	}
}

void APGPooledProjectile::BeginDestroy()
{
	// 디버그 로그
	UE_LOG(LogTemp, Log, TEXT("PooledProjectile BeginDestroy - InUse: %s, Pool: %s"), 
		bInUse ? TEXT("True") : TEXT("False"),
		OwnerPool ? *OwnerPool->GetName() : TEXT("NULL"));
	
	// Pool 참조 해제
	OwnerPool = nullptr;
	
	Super::BeginDestroy();
}