// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "GameFramework/PlayerController.h"
#include "PGPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class PGACTOR_API APGPlayerController : public APlayerController, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

private:
	FGenericTeamId PlayerTeamId;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;
	
public:
	APGPlayerController(const FObjectInitializer& ObjectInitializer);
	
public:
	// IGenericTeamAgentInterface
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	virtual FGenericTeamId GetGenericTeamId() const override { return PlayerTeamId; }

protected:
	virtual void BeginPlay() override;
};
