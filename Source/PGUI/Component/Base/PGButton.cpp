// Fill out your copyright notice in the Description page of Project Settings.

#include "PGButton.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"


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

	// World와 PlayerController 가져오기
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
	if (!PlayerController)
	{
		return;
	}

	// 버튼의 Viewport 좌표 계산
	FGeometry ButtonGeometry = GetCachedGeometry();
	FVector2D LocalSize = ButtonGeometry.GetLocalSize();
	FVector2D ButtonCenter = LocalSize * 0.5f; // 버튼 중심
	
	// LocalToViewport를 사용하여 올바른 Viewport 좌표 얻기
	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::LocalToViewport(World, ButtonGeometry, ButtonCenter, PixelPosition, ViewportPosition);

	// 화면 좌표를 월드 좌표로 변환 (Pixel 좌표 사용)
	FVector WorldLocation;
	FVector WorldDirection;
	if (PlayerController->DeprojectScreenPositionToWorld(PixelPosition.X, PixelPosition.Y, WorldLocation, WorldDirection))
	{
		// 카메라 앞 일정 거리에 VFX 스폰
		FVector SpawnLocation = WorldLocation + (WorldDirection * EffectSpawnDistance);
		
		// Niagara System 스폰
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ClickEffectSystem,
			SpawnLocation,
			FRotator::ZeroRotator,
			FVector(EffectScale),
			true,  // bAutoDestroy
			true,  // bAutoActivate
			ENCPoolMethod::AutoRelease,
			true   // bPreCullCheck
		);
	}
}

void UPGButton::OnButtonClicked()
{
	PlayClickEffect();
}
