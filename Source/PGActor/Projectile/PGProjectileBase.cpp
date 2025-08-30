// Fill out your copyright notice in the Description page of Project Settings.


#include "PGProjectileBase.h"

#include "GameFramework/ProjectileMovementComponent.h"

// Sets default values
APGProjectileBase::APGProjectileBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

void APGProjectileBase::Fire(const FVector& StartLocation, const FVector& Direction, float Speed, float InDamage)
{
	SetActorLocation(StartLocation);
	SetActorRotation(Direction.Rotation());
    
	MovementComponent->SetVelocityInLocalSpace(FVector::ForwardVector * Speed);
	Damage = InDamage;
	
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	
	// 수명 타이머
	GetWorldTimerManager().SetTimer(LifeTimeHandle, this, 
		&ThisClass::OnLifeTimeExpired, LifeTime, false);
}

void APGProjectileBase::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	OnHit.Broadcast(OtherActor, Hit);
    
	// [TODO]데미지 적용 로직
	if (OtherActor && OtherActor->CanBeDamaged())
	{
		// UGameplayStatics::ApplyPointDamage(...);
	}
}

void APGProjectileBase::OnLifeTimeExpired()
{
	Destroy();
}
