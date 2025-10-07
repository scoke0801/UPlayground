// Fill out your copyright notice in the Description page of Project Settings.


#include "PGAreaOfEffectBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/BoxComponent.h"
#include "PGAbilitySystem/Abilities/Util/PGAbilityBPLibrary.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/AreaOfEffect/PGAreaOfEffectDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayEventTags.h"

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
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionBox->SetupAttachment(GetRootComponent());
	
	// VFX 컴포넌트
	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
	NiagaraComponent->SetupAttachment(GetRootComponent());

	// Legacy Particle VFX 컴포넌트
	ParticleComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleComponent"));
	ParticleComponent->SetupAttachment(GetRootComponent());
	ParticleComponent->bAutoActivate = false;
	
	// 충돌 이벤트 바인딩
	CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnEffectOverlapped);
	CollisionBox->OnComponentEndOverlap.AddDynamic(this, &ThisClass::OnEffectEndOverlap);
}

APGAreaOfEffectBase* APGAreaOfEffectBase::Fire(AActor* InShooterActor, const FVector& StartLocation, int32 EffectId)
{
	UPGDataTableManager* DataManager = PGData();
	if (nullptr == DataManager)
	{
		return nullptr;
	}
	
	FPGAreaOfEffectDataRow* Data = DataManager->GetRowData<FPGAreaOfEffectDataRow>(EffectId);
	if (nullptr == Data || nullptr == InShooterActor)
	{
		return nullptr;
	}

	UWorld* World =  InShooterActor->GetWorld();
	if (nullptr == World)
	{
		return nullptr;
	}

	// Actor 생성
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InShooterActor;
	SpawnParams.Instigator = Cast<APawn>(InShooterActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APGAreaOfEffectBase* AOEActor = World->SpawnActor<APGAreaOfEffectBase>(
		APGAreaOfEffectBase::StaticClass(),
		StartLocation,
		FRotator::ZeroRotator,
		SpawnParams
	);
	
	if (!AOEActor)
	{
		return nullptr;
	}
	
	// 데이터 적용
	AOEActor->LifeTime = Data->LifeTime;
	AOEActor->DamageTickInterval = Data->TickInterval;
	
	// CollisionBox 크기 설정
	if (AOEActor->CollisionBox)
	{
		AOEActor->CollisionBox->SetBoxExtent(Data->BoxExtent);
	}
	
	
	if (EPGEffectType::Niagara == Data->EffectType)
	{
		UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(Data->NiagaraSystem.LoadSynchronous());
		if (NiagaraSystem && AOEActor->NiagaraComponent)
		{
			AOEActor->NiagaraComponent->SetAsset(NiagaraSystem);
			AOEActor->NiagaraComponent->SetVisibility(true);
			AOEActor->NiagaraComponent->Activate(true);
		}
			
		// ParticleComponent 비활성화
		if (AOEActor->ParticleComponent)
		{
			AOEActor->ParticleComponent->SetVisibility(false);
		}
	}
	else if (EPGEffectType::Legacy == Data->EffectType)
	{
		UParticleSystem* ParticleSystem = Cast<UParticleSystem>(Data->ParticleSystem.LoadSynchronous());

		if (ParticleSystem && AOEActor->ParticleComponent)
		{
			AOEActor->ParticleComponent->SetTemplate(ParticleSystem);
			AOEActor->ParticleComponent->SetVisibility(true);
			AOEActor->ParticleComponent->Activate(true);
		}
		
		// NiagaraComponent 비활성화
		if (AOEActor->NiagaraComponent)
		{
			AOEActor->NiagaraComponent->SetVisibility(false);
		}
	}
		
	AOEActor->Fire(InShooterActor, StartLocation);

	return AOEActor;
}

void APGAreaOfEffectBase::OnEffectOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult& Hit)
{
	if (nullptr == Shooter || nullptr == OtherActor)
	{
		return;
	}
	
	if (false == UPGAbilityBPLibrary::IsTargetActorHostile(Shooter ,OtherActor))
	{
		return;
	}
	
	if (APawn* CastedPawn = Cast<APawn>(OtherActor))
	{
		FGameplayEventData Data;
		Data.Instigator = Shooter;
		Data.Target = CastedPawn;
				
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			CastedPawn,
			PGGamePlayTags::Shared_Event_HitReact,
			Data);
		
		OverlappedPawn = CastedPawn;
		GetWorldTimerManager().SetTimer(DamageTickTimerHandle, this, 
			&ThisClass::OnDamageTicked, DamageTickInterval, true);
	}
}

void APGAreaOfEffectBase::OnEffectEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int OtherBodyIndex)
{
	GetWorldTimerManager().ClearTimer(DamageTickTimerHandle);
}

void APGAreaOfEffectBase::OnLifeTimeExpired()
{
	Destroy();
}

void APGAreaOfEffectBase::Fire(AActor* InShooterActor, const FVector& StartLocation)
{
	SetActorLocation(StartLocation);

	Shooter = InShooterActor;
	
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
	// 수명 타이머
	GetWorldTimerManager().SetTimer(LifeTimeHandle, this, 
		&ThisClass::OnLifeTimeExpired, LifeTime, true);
}

void APGAreaOfEffectBase::OnDamageTicked()
{
	FGameplayEventData Data;
	Data.Instigator = Shooter;
	Data.Target = OverlappedPawn;
				
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		OverlappedPawn,
		PGGamePlayTags::Shared_Event_HitReact,
		Data);
}
