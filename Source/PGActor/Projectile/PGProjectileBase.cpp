// Fill out your copyright notice in the Description page of Project Settings.


#include "PGProjectileBase.h"

#include "NiagaraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

// Sets default values
APGProjectileBase::APGProjectileBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// 충돌 컴포넌트
	ProjectileCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ProjectileCollisionBox"));
	SetRootComponent(ProjectileCollisionBox);
	
	ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	ProjectileCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// 메시 컴포넌트
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(GetRootComponent());
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	ProjectileNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ProjectileNiagaraComponent"));
	ProjectileNiagaraComponent->SetupAttachment(GetRootComponent());
	
	// 움직임 컴포넌트
	MovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComponent"));
	MovementComponent->bRotationFollowsVelocity = true;
	MovementComponent->InitialSpeed = 1000.0f;
	MovementComponent->MaxSpeed = 1000.0f;
	MovementComponent->ProjectileGravityScale = 0.f;

	// 충돌 이벤트 바인딩
	ProjectileCollisionBox->OnComponentHit.AddDynamic(this, &ThisClass::OnProjectileHit);
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
