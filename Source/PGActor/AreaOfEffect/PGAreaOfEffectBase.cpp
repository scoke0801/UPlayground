// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAreaOfEffectBase.h"

#include "NiagaraComponent.h"
#include "Components/BoxComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/AreaOfEffect/PGAreaOfEffectDataRow.h"

// Sets default values
APGAreaOfEffectBase::APGAreaOfEffectBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	Root = CreateDefaultSubobject<USceneComponent>("Root");
	SetRootComponent(Root);

	// 충돌 컴포넌트
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	
	CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetupAttachment(GetRootComponent());
	
	// VFX 컴포넌트
	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
	NiagaraComponent->SetupAttachment(GetRootComponent());
	
	// 충돌 이벤트 바인딩
	CollisionBox->OnComponentHit.AddDynamic(this, &ThisClass::OnEffectHit);
	CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnEffectOverlapped);
}

APGAreaOfEffectBase* APGAreaOfEffectBase::Fire(AActor* InShooterActor, const FVector& StartLocation,
	int32 EffectId,
	float InDamage)
{
	FPGAreaOfEffectDataRow* Data = PGData()->GetRowData<FPGAreaOfEffectDataRow>(EffectId);
	if (nullptr != Data)
	{
		return nullptr;
	}

	if (APGAreaOfEffectBase* AOEActor = NewObject<APGAreaOfEffectBase>())
	{
		if (EPGEffectType::Niagara == Data->EffectType)
		{
			// TODO: NiagaraSystem, 방식 사용하여 이펙트 처리
		}
		else if (EPGEffectType::Legacy == Data->EffectType)
		{
			// TODO: UParticleSystem 방식 사용하여 이펙트 처리
		}
		
		AOEActor->Fire(InShooterActor, StartLocation, InDamage);

		return AOEActor;
	}

	return nullptr;
}

void APGAreaOfEffectBase::OnEffectHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
}

void APGAreaOfEffectBase::OnEffectOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult& Hit)
{
	if (UPGAbilityBPLibrary::IsTargetActorHostile(Shooter ,OtherActor))
	{
		return;
	}
	
	// StaticObject 확인
	if (Hit.Component.IsValid())
	{
		ECollisionChannel ObjectType = Hit.Component->GetCollisionObjectType();
		if (ObjectType == ECC_WorldStatic || ObjectType == ECC_WorldDynamic)
		{
			Destroy();
			return;
		}
	}
}

void APGAreaOfEffectBase::OnLifeTimeExpired()
{
	Destroy();
}

void APGAreaOfEffectBase::Fire(AActor* InShooterActor, const FVector& StartLocation, float InDamage)
{
	SetActorLocation(StartLocation);

	Shooter = InShooterActor;
	
	Damage = InDamage;
	
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	
	// 수명 타이머
	GetWorldTimerManager().SetTimer(LifeTimeHandle, this, 
		&ThisClass::OnLifeTimeExpired, LifeTime, false);
}
