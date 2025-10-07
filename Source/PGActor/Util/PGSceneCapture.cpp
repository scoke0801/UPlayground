// Fill out your copyright notice in the Description page of Project Settings.


#include "PGSceneCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"

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
	
	// 조명 컴포넌트 추가 - Key Light (메인 조명)
	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(RootComponent);
	KeyLight->SetIntensity(3.0f);
	KeyLight->SetLightColor(FLinearColor(1.0f, 0.95f, 0.9f)); // 약간 따뜻한 색
	KeyLight->SetRelativeRotation(FRotator(-30.0f, 45.0f, 0.0f)); // 위에서 비추는 각도
	KeyLight->SetCastShadows(false); // 성능 최적화
	
	// 환경광 추가 - Sky Light
	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(RootComponent);
	SkyLight->SetIntensity(1.0f);
	SkyLight->SetLightColor(FLinearColor(0.8f, 0.85f, 1.0f)); // 약간 차가운 환경광
	SkyLight->bLowerHemisphereIsBlack = false;
	SkyLight->SetCastShadows(false);
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
	FVector CaptureLocation = HeadLocation + FVector(70.0f, -20.0f, 5.0f);
    
	// 카메라 회전: 캐릭터 얼굴을 바라봄
	FRotator CaptureRotation = (HeadLocation - CaptureLocation).Rotation();
	
	SetActorLocation(CaptureLocation);
	SetActorRotation(CaptureRotation);

	// 캐릭터를 카메라 방향으로 회전
	FVector DirectionToCamera = CaptureLocation - TargetCharacter->GetActorLocation();
	DirectionToCamera.Z = 0.0f; // 수평 방향만 사용
	
	FRotator OriginalRotation = TargetCharacter->GetActorRotation();
	FRotator LookAtRotation = DirectionToCamera.Rotation();
	LookAtRotation.Yaw -= 20.0f;
	TargetCharacter->SetActorRotation(LookAtRotation);

	// 특정 캐릭터만 렌더링
	SceneCaptureComponent->ShowOnlyActors.Empty();
	SceneCaptureComponent->ShowOnlyActors.Add(TargetCharacter);
	SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	
	// ShowFlags 설정 - 조명 활성화
	SceneCaptureComponent->ShowFlags.SetLighting(true);
	SceneCaptureComponent->ShowFlags.SetDynamicShadows(false); // 성능 최적화
	SceneCaptureComponent->ShowFlags.SetAtmosphere(false);
	SceneCaptureComponent->ShowFlags.SetShadowFrustums(false);
	SceneCaptureComponent->ShowFlags.SetVirtualShadowMapPersistentData(false);
	SceneCaptureComponent->ShowFlags.SetFog(false);
	SceneCaptureComponent->ShowFlags.SetVolumetricFog(false);
	SceneCaptureComponent->ShowFlags.SetSkyLighting(true);
	SceneCaptureComponent->ShowFlags.SetGlobalIllumination(true);
	
	// Post Process 설정으로 밝기 조정
	SceneCaptureComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	SceneCaptureComponent->PostProcessSettings.AutoExposureBias = 5.5f; // 충분히 밝게
	
	// Ambient Occlusion 끄기 (더 밝게)
	SceneCaptureComponent->PostProcessSettings.bOverride_AmbientOcclusionIntensity = true;
	SceneCaptureComponent->PostProcessSettings.AmbientOcclusionIntensity = 0.0f;
        
	SceneCaptureComponent->CaptureScene();

	// 원복해둔다.
	TargetCharacter->SetActorRotation(OriginalRotation);
}

UTextureRenderTarget2D* APGSceneCapture::GetRenderTarget() const
{
	return RenderTarget;
}

void APGSceneCapture::SetRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	this->RenderTarget = InRenderTarget;
}
