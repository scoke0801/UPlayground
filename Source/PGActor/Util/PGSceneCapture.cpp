// Fill out your copyright notice in the Description page of Project Settings.


#include "PGSceneCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

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
	SceneCaptureComponent->FOVAngle = 45.0f; // 좁은 FOV로 얼굴 줌인
}

void APGSceneCapture::CaptureCurrentScene(AActor* TargetCharacter)
{
	if (!TargetCharacter || !RenderTarget)
	{
		return;
	}
	
	// Render Target 설정
	SceneCaptureComponent->TextureTarget = RenderTarget;

	// 캐릭터의 머리/얼굴 위치 가져오기
	FVector HeadLocation = TargetCharacter->GetActorLocation();
	
	// Character인 경우 머리 본 위치 사용
	ACharacter* Character = Cast<ACharacter>(TargetCharacter);
	if (Character && Character->GetMesh())
	{
		// "head" 본이 있으면 해당 위치 사용, 없으면 기본 위치에서 높이 조정
		FName HeadBoneName = FName(TEXT("head"));
		if (Character->GetMesh()->GetBoneIndex(HeadBoneName) != INDEX_NONE)
		{
			HeadLocation = Character->GetMesh()->GetSocketLocation(HeadBoneName);
		}
		else
		{
			// 머리 본이 없으면 캐릭터 높이 기준으로 추정 (일반적으로 약 90% 높이)
			float CharacterHeight = Character->GetSimpleCollisionCylinderExtent().Z * 2.0f;
			HeadLocation.Z += CharacterHeight * 0.75f;
		}
	}
	else
	{
		// Character가 아닌 경우 기본 높이 추가
		HeadLocation.Z += 150.0f;
	}

	// 카메라 위치: 얼굴 정면에서 적당한 거리
	// 전방 100cm, 약간 위에서 (15cm) 촬영하여 자연스러운 각도
	FVector CaptureLocation = HeadLocation + FVector(100.0f, 0.0f, 15.0f);
    
	// 카메라 회전: 캐릭터 얼굴을 바라봄
	FRotator CaptureRotation = (HeadLocation - CaptureLocation).Rotation();
	
	SetActorLocation(CaptureLocation);
	SetActorRotation(CaptureRotation);

	// 캐릭터를 카메라 방향으로 회전
	FVector DirectionToCamera = CaptureLocation - TargetCharacter->GetActorLocation();
	DirectionToCamera.Z = 0.0f; // 수평 방향만 사용
	FRotator LookAtRotation = DirectionToCamera.Rotation();
	TargetCharacter->SetActorRotation(LookAtRotation);

	// 특정 캐릭터만 렌더링
	SceneCaptureComponent->ShowOnlyActors.Empty();
	SceneCaptureComponent->ShowOnlyActors.Add(TargetCharacter);
	SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	
	// ShowFlags 설정으로 배경 제거 및 최적화
	SceneCaptureComponent->ShowFlags.SetAtmosphere(false);
	SceneCaptureComponent->ShowFlags.SetFog(false);
	SceneCaptureComponent->ShowFlags.SetVolumetricFog(false);
        
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
