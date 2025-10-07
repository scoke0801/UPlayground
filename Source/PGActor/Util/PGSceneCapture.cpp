// Fill out your copyright notice in the Description page of Project Settings.


#include "PGSceneCapture.h"

#include "Components/SceneCaptureComponent2D.h"

// Sets default values
APGSceneCapture::APGSceneCapture()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	// Scene Capture Component 생성
	SceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCaptureComponent"));
	RootComponent = SceneCaptureComponent;
    
	// 기본 설정
	SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
	SceneCaptureComponent->CaptureSource = SCS_FinalColorLDR;
	SceneCaptureComponent->bCaptureEveryFrame = false;
}

void APGSceneCapture::CaptureCurrentScene(AActor* TargetCharacter)
{
	if (!TargetCharacter || !RenderTarget)
	{
		return;
	}
	
	// Render Target 설정
	SceneCaptureComponent->TextureTarget = RenderTarget;

	// 캐릭터를 바라보도록 카메라 위치 설정
	FVector CharacterLocation = TargetCharacter->GetActorLocation();
	FVector CaptureLocation = CharacterLocation + FVector(200.0f, 0.0f, 50.0f);
    
	SetActorLocation(CaptureLocation);
	SetActorRotation((CharacterLocation - CaptureLocation).Rotation());

	// 한 프레임 캡처
	SceneCaptureComponent->ShowOnlyActors.Empty();
	SceneCaptureComponent->ShowOnlyActors.Add(TargetCharacter);
	SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        
	SceneCaptureComponent->CaptureScene();
}

UTextureRenderTarget2D* APGSceneCapture::GetRenderTarget() const
{
	return RenderTarget;
}

void APGSceneCapture::SetRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	this->RenderTarget = InRenderTarget;
}
