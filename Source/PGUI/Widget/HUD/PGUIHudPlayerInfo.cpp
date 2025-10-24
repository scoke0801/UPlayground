// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIHudPlayerInfo.h"

#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Util/PGSceneCapture.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGShared/Shared/Message/Stat/PGStatUpdateEventData.h"

void UPGUIHudPlayerInfo::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UPGUIHudPlayerInfo::NativeConstruct()
{
	Super::NativeConstruct();
	
	// Player 정보 가져와야 한다...
	StatUpdateHandle = PGMessage()->RegisterDelegate(EPGPlayerMessageType::StatUpdate,
		this, &ThisClass::OnStatUpdate);
	
	SpawnedHandle = PGMessage()->RegisterDelegate(EPGPlayerMessageType::Spawned,
		this, &ThisClass::OnPlayerSpawned);

//	CapturePlayerImage();
}

void UPGUIHudPlayerInfo::NativeDestruct()
{
	Super::NativeDestruct();

	if (UPGMessageManager* MessageManager = PGMessage())
	{
		MessageManager->UnregisterDelegate(EPGPlayerMessageType::StatUpdate, StatUpdateHandle);
		MessageManager->UnregisterDelegate(EPGPlayerMessageType::Spawned, SpawnedHandle);
	}
}

void UPGUIHudPlayerInfo::OnStatUpdate(const IPGEventData* InEventData)
{
	const FPGStatUpdateEventData* EventData = static_cast<const FPGStatUpdateEventData*>(InEventData);
	if (nullptr == EventData)
	{
		return;
	}

	if (EPGStatType::Health == EventData->StatType)
	{
		
	}
}

void UPGUIHudPlayerInfo::OnPlayerSpawned(const IPGEventData* InEventData)
{
	CapturePlayerImage();
}

void UPGUIHudPlayerInfo::CapturePlayerImage()
{
	UWorld* World = GetWorld();
	if (!World || !PlayerImage)
	{
		return;
	}

	// 플레이어 캐릭터 가져오기
	APGCharacterPlayer* PlayerCharacter = Cast<APGCharacterPlayer>(World->GetFirstPlayerController()->GetPawn());
	if (!PlayerCharacter)
	{
		return;
	}

	// Scene Capture Actor를 화면 밖 위치에 스폰 (메인 씬에 영향 없도록)
	FVector HiddenLocation = FVector(0.0f, 0.0f, -100000.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	APGSceneCapture* SceneCapture = World->SpawnActor<APGSceneCapture>(
		APGSceneCapture::StaticClass(),
		HiddenLocation,
		FRotator::ZeroRotator,
		SpawnParams
	);
	
	if (!SceneCapture)
	{
		return;
	}

	// Render Target 생성 및 초기화 (해상도 높임)
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(SceneCapture);
	if (RenderTarget)
	{
		RenderTarget->InitAutoFormat(256, 256);
		RenderTarget->ClearColor = FLinearColor::Transparent;
		RenderTarget->bAutoGenerateMips = false;
		RenderTarget->UpdateResourceImmediate(true);
		
		SceneCapture->SetRenderTarget(RenderTarget);

		// 캡처 실행
		SceneCapture->CaptureCurrentScene(PlayerCharacter);

		// Material Instance Dynamic 생성 및 텍스처 설정
		if (!PlayerImageMaterial)
		{
			// Image 브러시에서 Material 가져오기
			UMaterialInterface* BaseMaterial = Cast<UMaterialInterface>(PlayerImage->GetBrush().GetResourceObject());
			if (BaseMaterial)
			{
				PlayerImageMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			}
		}

		if (PlayerImageMaterial)
		{
			// 원형 마스크 머티리얼의 텍스처 파라미터에 RenderTarget 설정
			PlayerImageMaterial->SetTextureParameterValue(TEXT("MainTexture"), RenderTarget);
			PlayerImage->SetBrushFromMaterial(PlayerImageMaterial);
		}

		// 캡처 완료 후 Actor 제거
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(TimerHandle,[SceneCapture]()
		{
			if (SceneCapture && IsValid(SceneCapture))
			{
				SceneCapture->Destroy();
			}
		}, 0.1f, false);
	}
}
