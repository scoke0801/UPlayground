// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PGSceneCapture.generated.h"

UCLASS()
class PGACTOR_API APGSceneCapture : public AActor
{
	GENERATED_BODY()

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PG")
	USceneCaptureComponent2D* SceneCaptureComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG")
	UTextureRenderTarget2D* RenderTarget;

public:	
	// Sets default values for this actor's properties
	APGSceneCapture();

public:	
	void CaptureCurrentScene(AActor* TargetCharacter);

	UTextureRenderTarget2D* GetRenderTarget() const;
	void SetRenderTarget(UTextureRenderTarget2D* InRenderTarget);
};
