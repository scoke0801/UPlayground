// Fill out your copyright notice in the Description page of Project Settings.

#include "PGPooledProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "PGProjectilePool.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGShared/Shared/Debug/PGDebugHelper.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"

void APGPooledProjectile::Fire(AActor* InShooterActor, const FVector& StartLocation, const FVector& Direction, float Speed, float InDamage)
{
	// MovementComponent 재활성화
	if (MovementComponent)
	{
		MovementComponent->Activate();
		MovementComponent->SetUpdatedComponent(RootComponent);
	}

	Super::Fire(InShooterActor, StartLocation, Direction, Speed, InDamage);
	
	bInUse = true;
}

void APGPooledProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::OnProjectileHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);

	// 히트 후 풀로 반환
	ReturnToPool();
}

void APGPooledProjectile::OnProjectileOverlapped(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult& Hit)
{
	if (APawn* CastedPawn = Cast<APawn>(OtherActor))
	{
		if (UPGAbilityBPLibrary::IsTargetActorHostile(Shooter ,OtherActor))
		{
			FGameplayEventData Data;
			Data.Instigator = Shooter;
			Data.Target = CastedPawn;
				
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
            	CastedPawn,
            	PGGamePlayTags::Shared_Event_HitReact,
            	Data);

			// 히트 후 풀로 반환
			ReturnToPool();
			return;
		}
	}
	
	// StaticObject 확인
	if (Hit.Component.IsValid())
	{
		ECollisionChannel ObjectType = Hit.Component->GetCollisionObjectType();

		if (ObjectType == ECC_WorldStatic)
		{
			ReturnToPool();
			return;
		}
	}
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
}

void APGPooledProjectile::BeginDestroy()
{
	// Pool 참조 해제
	OwnerPool = nullptr;
	
	Super::BeginDestroy();
}