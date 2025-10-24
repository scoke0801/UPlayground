// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterBase.h"

#include "MotionWarpingComponent.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "MotionWarpingComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PGActor/Components/Stat/PGStatComponent.h"

const FName DissolveEdgeColorName = FName("DissolveEdgeColor");
const FName DissolveParticleColorName = FName("DissolveParticleColor");

// Sets default values
APGCharacterBase::APGCharacterBase()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	GetMesh()->bReceivesDecals = false;
	
	AbilitySystemComponent = CreateDefaultSubobject<UPGAbilitySystemComponent>(TEXT("PGAbilitySystemComponent"));

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

void APGCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (nullptr != GetStatComponent())
	{
		GetStatComponent()->InitData(CharacterTID);

		UpdateMovementSpeed();
	}
	// SkillHandler = FPGHandler::Create<FPGSkillHandler>();
}

void APGCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (nullptr != SkillHandler)
	{
		delete SkillHandler;
	}
	Super::EndPlay(EndPlayReason);
}

void APGCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (nullptr != AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void APGCharacterBase::OnDied()
{
}

UAbilitySystemComponent* APGCharacterBase::GetAbilitySystemComponent() const
{
	return GetPGAbilitySystemComponent();
}

UPGAbilitySystemComponent* APGCharacterBase::GetPGAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void APGCharacterBase::PlayVFX(UNiagaraSystem* ToPlayTemplate)
{
}

void APGCharacterBase::PlayDeathDissolveVFX(UNiagaraSystem* ToPlayTemplate)
{
	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
						ToPlayTemplate, GetMesh(), NAME_None,
						FVector::ZeroVector,FRotator::ZeroRotator, EAttachLocation::Type::KeepRelativeOffset,
						true,true);


	UMaterialInstanceDynamic* DynamicMaterial = GetMesh()->CreateDynamicMaterialInstance(0, GetMesh()->GetMaterial(0));
	if (!DynamicMaterial)
	{
		return;
	}

	FLinearColor DissolveEdgeColor;
	if (DynamicMaterial->GetVectorParameterValue(DissolveEdgeColorName, DissolveEdgeColor))
	{
		NiagaraComp->SetVariableLinearColor(DissolveParticleColorName, DissolveEdgeColor);
	}
	
	NiagaraComp->SetCustomTimeDilation(3.0f);
}

void APGCharacterBase::UpdateMovementSpeed()
{
	UPGStatComponent* StatComp = GetStatComponent();
	if (nullptr == StatComp)
	{
		return;
	}

	int32 MoveSpeed= StatComp->GetStat(EPGStatType::MovementSpeed);
	if (0 < MoveSpeed)
	{
		if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
		{
			MovementComp->MaxWalkSpeed = MoveSpeed;
		}
	}
}

