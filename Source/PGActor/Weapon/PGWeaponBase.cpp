// Fill out your copyright notice in the Description page of Project Settings.


#include "PGWeaponBase.h"

#include "Components/BoxComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGData/PGDataTableManager.h"

// Sets default values
APGWeaponBase::APGWeaponBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));

	SetRootComponent(WeaponMesh);

	WeaponCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponCollisionBox"));
	WeaponCollisionBox->SetupAttachment(GetRootComponent());
	WeaponCollisionBox->SetBoxExtent(FVector(20.0f));
	WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this,&ThisClass::OnCollisionBoxBeginOverlap);
	WeaponCollisionBox->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::OnCollisionBoxEndOverlap);
}

void APGWeaponBase::OnCollisionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* WeaponOwningPawn = GetInstigator<APawn>();

	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		if (UPGAbilityBPLibrary::IsTargetPawnHostile(WeaponOwningPawn,HitPawn))
		{
			OnWeaponHitTarget.ExecuteIfBound(OtherActor);
		}
	}
}

void APGWeaponBase::OnCollisionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	APawn* WeaponOwningPawn = GetInstigator<APawn>();

	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		if (UPGAbilityBPLibrary::IsTargetPawnHostile(WeaponOwningPawn,HitPawn))
		{
			OnWeaponPullTarget.ExecuteIfBound(OtherActor);
		}
	}
}

void APGWeaponBase::AssignGrantAbilitySpecHandles(const TArray<FGameplayAbilitySpecHandle>& InSpecHandles)
{
	AbilitySpecHandles = InSpecHandles;

	if (UPGDataTableManager* dataTableManager = GetGameInstance()->GetSubsystem<UPGDataTableManager>())
	{
		// TODO
	}
}

TArray<FGameplayAbilitySpecHandle> APGWeaponBase::GetGrantedAbilitySpecHandles() const
{
	return AbilitySpecHandles;
}

