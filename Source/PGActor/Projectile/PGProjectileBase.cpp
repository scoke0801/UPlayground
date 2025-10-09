// Fill out your copyright notice in the Description page of Project Settings.


#include "PGProjectileBase.h"

#include "NiagaraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGActor/Characters/PGCharacterBase.h"
#include "PGShared/Shared/Debug/PGDebugHelper.h"

// Sets default values
APGProjectileBase::APGProjectileBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	Root = CreateDefaultSubobject<USceneComponent>("Root");
	SetRootComponent(Root);

	// 충돌 컴포넌트
	ProjectileCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ProjectileCollisionBox"));
	
	ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	ProjectileCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProjectileCollisionBox->SetupAttachment(GetRootComponent());
	
	// 메시 컴포넌트
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetupAttachment(GetRootComponent());

	// VFX 컴포넌트
	ProjectileNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ProjectileNiagaraComponent"));
	ProjectileNiagaraComponent->SetupAttachment(GetRootComponent());
	
	// 움직임 컴포넌트
	MovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComponent"));
	MovementComponent->bRotationFollowsVelocity = true;
	MovementComponent->InitialSpeed = 2000.0f;
	MovementComponent->MaxSpeed = 100000.0f;
	MovementComponent->ProjectileGravityScale = 0.f;

	// 충돌 이벤트 바인딩
	ProjectileCollisionBox->OnComponentHit.AddDynamic(this, &ThisClass::OnProjectileHit);
	ProjectileCollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnProjectileOverlapped);
}

void APGProjectileBase::Fire(AActor* InShooterActor, const FVector& StartLocation, const FVector& Direction, float Speed,
	float InDamage)
{
	SetActorLocation(StartLocation);
	SetActorRotation(Direction.Rotation());

	Shooter = InShooterActor;
	
	MovementComponent->SetVelocityInLocalSpace(FVector::ForwardVector * Speed);
	Damage = InDamage;
	
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	if (APGCharacterBase* Character = Cast<APGCharacterBase>(Shooter))
	{
		ProjectileCollisionBox->SetCollisionResponseToChannel(Character->GetCollisionChannel(), ECR_Ignore);
	}
	
	// 수명 타이머
	GetWorldTimerManager().SetTimer(LifeTimeHandle, this, 
		&ThisClass::OnLifeTimeExpired, LifeTime, false);
}

void APGProjectileBase::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
}

void APGProjectileBase::OnProjectileOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult& Hit)
{
	// 자기 자신이거나 발사자는 무시
	if (OtherActor == this || OtherActor == Shooter)
	{
		return;
	}
	
	// 적대적인 대상에게만 피해
	if (false == UPGAbilityBPLibrary::IsTargetActorHostile(Shooter, OtherActor))
	{
		return;
	}
	
	// 벽이나 바닥에 충돌 시
	if (Hit.Component.IsValid())
	{
		ECollisionChannel ObjectType = Hit.Component->GetCollisionObjectType();
		if (ObjectType == ECC_WorldStatic || ObjectType == ECC_WorldDynamic)
		{
			Destroy();
		}
	}
}

void APGProjectileBase::OnLifeTimeExpired()
{
	Destroy();
}
