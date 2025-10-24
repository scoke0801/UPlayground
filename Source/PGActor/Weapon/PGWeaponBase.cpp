// Fill out your copyright notice in the Description page of Project Settings.


#include "PGWeaponBase.h"

#include "Components/BoxComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Stat/PGWeaponDataRow.h"

// Sets default values
APGWeaponBase::APGWeaponBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>("Root");
	SetRootComponent(Root);
	
	StaticWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	StaticWeaponMesh->SetupAttachment(RootComponent);
	
	SkeletalWeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalWeaponMesh"));
	SkeletalWeaponMesh->SetupAttachment(RootComponent);

	WeaponCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponCollisionBox"));
	WeaponCollisionBox->SetupAttachment(StaticWeaponMesh);
	WeaponCollisionBox->SetBoxExtent(FVector(20.0f));
	WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this,&ThisClass::OnCollisionBoxBeginOverlap);
	WeaponCollisionBox->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::OnCollisionBoxEndOverlap);
	
	WeaponMeshType = EPGWeaponMeshType::StaticMesh;
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

UMeshComponent* APGWeaponBase::GetMeshComponent() const
{
	if (EPGWeaponMeshType::SkeletalMesh == WeaponMeshType)
	{
		return SkeletalWeaponMesh;
	}
	return StaticWeaponMesh;
}

int32 APGWeaponBase::GetWeaponStat(EPGStatType StatType) const
{
	if (FPGWeaponDataRow* Data = PGData()->GetRowData<FPGWeaponDataRow>(WeaponId))
	{
		if (Data->Stats.Contains(StatType))
		{
			return Data->Stats[StatType];
		}	
	}
	return 0;
}

#if WITH_EDITOR
void APGWeaponBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();
        
		// WeaponMeshType이 변경되었을 때
		if (PropertyName == GET_MEMBER_NAME_CHECKED(APGWeaponBase, WeaponMeshType))
		{
			if (EPGWeaponMeshType::SkeletalMesh == WeaponMeshType)
			{
				WeaponCollisionBox->AttachToComponent(SkeletalWeaponMesh, FAttachmentTransformRules::KeepRelativeTransform);
			}
			else
			{
				WeaponCollisionBox->AttachToComponent(StaticWeaponMesh, FAttachmentTransformRules::KeepRelativeTransform);
			}
		}
	}
}
#endif