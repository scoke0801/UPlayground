// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PGCharacterBase.generated.h"

UCLASS()
class UPLAYGROUND_API APGCharacterBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APGCharacterBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

};
