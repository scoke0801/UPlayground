// Fill out your copyright notice in the Description page of Project Settings.

#include "PGButton.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"

UPGButton::UPGButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 기본값 설정은 헤더에서 처리됨
}

void UPGButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	
	// 클릭 이벤트 바인딩 (한 번만 실행)
	if (!bIsEventBound)
	{
		OnClicked.AddDynamic(this, &UPGButton::OnButtonClicked);
		bIsEventBound = true;
	}
}

void UPGButton::PlayClickEffect()
{
	if (!ClickEffectSystem)
	{
		return;
	}
	
	// 렌더링 방식에 따라 분기
	if (bUseScreenSpace)
	{
		SpawnScreenSpaceEffect();
	}
	else
	{
		SpawnWorldSpaceEffect();
	}
}

void UPGButton::OnButtonClicked()
{
	PlayClickEffect();
}

// ========================================
// 스크린 스페이스 방식 (정확한 2D 위치)
// ========================================

void UPGButton::SpawnScreenSpaceEffect()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !GetWorld())
	{
		return;
	}
	
	// 1. 버튼의 화면 절대 위치 계산
	FGeometry ButtonGeometry = GetCachedGeometry();
	FVector2D LocalCenter = ButtonGeometry.GetLocalSize() * 0.5f + ParticleOffset;
	FVector2D ScreenPosition = ButtonGeometry.LocalToAbsolute(LocalCenter);
	
	// 2. Viewport 크기 가져오기
	int32 ViewportSizeX, ViewportSizeY;
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
	
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		return;
	}
	
	// 3. DPI 스케일 적용
	float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	ScreenPosition /= ViewportScale;
	
	// 4. 카메라 정보 가져오기
	APlayerCameraManager* CameraManager = PC->PlayerCameraManager;
	if (!CameraManager)
	{
		return;
	}
	
	FVector CameraLocation = CameraManager->GetCameraLocation();
	FRotator CameraRotation = CameraManager->GetCameraRotation();
	FVector CameraForward = CameraRotation.Vector();
	
	// 5. 화면 좌표를 월드 Ray로 변환
	FVector WorldLocation, WorldDirection;
	if (!UGameplayStatics::DeprojectScreenToWorld(PC, ScreenPosition, WorldLocation, WorldDirection))
	{
		return;
	}
	
	// 6. 카메라 앞 고정 거리에 배치 (UI와 동일 평면)
	// UI는 보통 카메라 앞 가까운 거리에 렌더링되므로, 같은 거리에 배치
	float DistanceFromCamera = 100.0f;  // UI와 동일한 거리
	FVector SpawnLocation = CameraLocation + CameraForward * DistanceFromCamera;
	
	// 7. 화면 좌표에 맞춰 좌우/상하 오프셋 적용
	FVector CameraRight = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);
	FVector CameraUp = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Z);
	
	// 화면 중앙으로부터의 오프셋 계산
	FVector2D ViewportCenter(ViewportSizeX * 0.5f, ViewportSizeY * 0.5f);
	FVector2D OffsetFromCenter = ScreenPosition - ViewportCenter;
	
	// FOV와 거리를 고려한 실제 월드 오프셋 계산
	float FOV = CameraManager->GetFOVAngle();
	float HalfFOVRadians = FMath::DegreesToRadians(FOV * 0.5f);
	float ViewportHeight = 2.0f * DistanceFromCamera * FMath::Tan(HalfFOVRadians);
	float ViewportWidth = ViewportHeight * (float(ViewportSizeX) / float(ViewportSizeY));
	
	float WorldOffsetX = (OffsetFromCenter.X / ViewportSizeX) * ViewportWidth;
	float WorldOffsetY = -(OffsetFromCenter.Y / ViewportSizeY) * ViewportHeight;  // Y는 반대
	
	SpawnLocation += CameraRight * WorldOffsetX + CameraUp * WorldOffsetY;
	
	// 8. 디버그 표시
	if (bDebugShowSpawnLocation)
	{
		DrawDebugSphere(GetWorld(), SpawnLocation, 10.0f, 12, FColor::Green, false, 2.0f);
		DrawDebugLine(GetWorld(), CameraLocation, SpawnLocation, FColor::Yellow, false, 2.0f);
	}
	
	// 9. Niagara 스폰 (카메라를 향하도록 회전)
	FRotator SpawnRotation = (CameraLocation - SpawnLocation).Rotation();
	
	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		ClickEffectSystem,
		SpawnLocation,
		SpawnRotation,
		ParticleScale,
		true,   // bAutoDestroy
		true,   // bAutoActivate
		ENCPoolMethod::None,
		true    // bPreCullCheck
	);
	
	if (NiagaraComp)
	{
		// 필요시 추가 설정
	}
}

UCanvasPanel* UPGButton::FindRootCanvasPanel() const
{
	UWidget* Current = const_cast<UPGButton*>(this);
	
	while (Current)
	{
		UPanelWidget* Parent = Current->GetParent();
		if (!Parent)
		{
			break;
		}
		
		if (UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(Parent))
		{
			return CanvasPanel;
		}
		
		Current = Parent;
	}
	
	return nullptr;
}

// ========================================
// 월드 스페이스 방식 (기존 방식)
// ========================================

void UPGButton::SpawnWorldSpaceEffect()
{
	FVector WorldLocation;
	FVector WorldDirection;
	
	if (GetButtonWorldLocation(WorldLocation, WorldDirection))
	{
		SpawnNiagaraAtLocation(WorldLocation, WorldDirection);
	}
}

bool UPGButton::GetButtonWorldLocation(FVector& OutWorldLocation, FVector& OutWorldDirection) const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return false;
	}
	
	// 1) 화면(뷰포트)상의 마우스 / 터치 위치 얻기
	float ScreenX = 0.f, ScreenY = 0.f;
	// GetMousePosition returns false if not available (e.g. gamepad)
	const bool bGotMouse = UWidgetLayoutLibrary::GetMousePositionScaledByDPI(PC,ScreenX, ScreenY);
	if (!bGotMouse)
	{
		// 대안: 마우스가 없으면 위젯의 중앙을 사용할 수도 있음
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		ScreenX = ViewportSize.X * 0.5f;
		ScreenY = ViewportSize.Y * 0.5f;
	}

	// 2) 스크린 좌표 -> 월드 오리진 및 방향으로 변환
	FVector WorldOrigin;
	FVector WorldDirection;
	if (!PC->DeprojectScreenPositionToWorld(ScreenX, ScreenY, WorldOrigin, WorldDirection))
	{
		UE_LOG(LogTemp, Warning, TEXT("Deproject failed."));
		return false;
	}
	else
	{
		// 3) 카메라 앞 적절한 거리(예: 500 단위)로 위치 결정
		const float SpawnDistance = 500.0f; // 필요에 따라 조정
		OutWorldLocation = WorldOrigin + WorldDirection * SpawnDistance;
		OutWorldDirection = WorldDirection;
		return true;
	}
	
	// 버튼의 Geometry 정보 가져오기
	FGeometry ButtonGeometry = GetCachedGeometry();
	
	// 버튼의 로컬 중심점 계산
	FVector2D LocalCenter = ButtonGeometry.GetLocalSize() * 0.5f;
	
	// 사용자 정의 오프셋 적용
	LocalCenter += ParticleOffset;
	
	// 화면 공간(픽셀) 좌표로 변환
	FVector2D ScreenPosition = ButtonGeometry.LocalToAbsolute(LocalCenter);
	
	// DPI 스케일 적용
	float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	ScreenPosition /= ViewportScale;
	
	// 화면 좌표를 월드 좌표로 변환 (Deproject)
	bool bSuccess = UGameplayStatics::DeprojectScreenToWorld(
		PC,
		ScreenPosition,
		OutWorldLocation,
		OutWorldDirection
	);
	
	if (bSuccess)
	{
		// UI 앞쪽(카메라 방향)으로 오프셋 적용
		OutWorldLocation += OutWorldDirection * ParticleZOffset;
		
		// 디버그: 스폰 위치 표시
		if (bDebugShowSpawnLocation)
		{
			DrawDebugSphere(
				GetWorld(),
				OutWorldLocation,
				10.0f,
				12,
				FColor::Yellow,
				false,
				2.0f
			);
		}
	}
	
	return bSuccess;
}

void UPGButton::SpawnNiagaraAtLocation(const FVector& WorldLocation, const FVector& WorldDirection)
{
	if (!ClickEffectSystem || !GetWorld())
	{
		return;
	}
	
	// Niagara System을 월드에 스폰 (One-Shot)
	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		ClickEffectSystem,
		WorldLocation,
		FRotator::ZeroRotator,
		ParticleScale,
		true,   // bAutoDestroy: 재생 완료 후 자동 삭제
		true,   // bAutoActivate: 즉시 재생
		ENCPoolMethod::None,  // 풀링 사용 안 함
		true    // bPreCullCheck
	);
	
	if (NiagaraComp)
	{
		// 필요시 추가 설정
	}
}
