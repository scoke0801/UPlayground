// Fill out your copyright notice in the Description page of Project Settings.


#include "PGCharacterBase.h"

#include "PGAbilitySystem/PGAbilitySystemComponent.h"

// Sets default values
APGCharacterBase::APGCharacterBase()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	GetMesh()->bReceivesDecals = false;
	
	AbilitySystemComponent = CreateDefaultSubobject<UPGAbilitySystemComponent>(TEXT("PGAbilitySystemComponent"));
}

void APGCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (nullptr != AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

UAbilitySystemComponent* APGCharacterBase::GetAbilitySystemComponent() const
{
	return GetPGAbilitySystemComponent();
}

UPGAbilitySystemComponent* APGCharacterBase::GetPGAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

