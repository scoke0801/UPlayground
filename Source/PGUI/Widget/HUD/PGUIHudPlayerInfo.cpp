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

	// Player 정보 가져와야 한다...
	PGMessage()->RegisterDelegate(EPGPlayerMessageType::StatUpdate,
		this, &ThisClass::OnStatUpdate);
}

void UPGUIHudPlayerInfo::NativeConstruct()
{
	Super::NativeConstruct();
	
	CapturePlayerImage();
}

void UPGUIHudPlayerInfo::OnStatUpdate(const IPGEventData* InEventData)
{
	const FPGStatUpdateEventData* EventData = static_cast<const FPGStatUpdateEventData*>(InEventData);
	if (nullptr == EventData)
	{
		return;
	}

	if (EPGStatType::Hp == EventData->StatType)
	{
		
	}
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
		// 512x512로 품질 향상 (필요시 256으로 낮출 수 있음)
		RenderTarget->InitAutoFormat(512, 512);
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
	}

	// 캡처 완료 후 Actor 제거 (다음 프레임에)
	World->GetTimerManager().SetTimerForNextTick([SceneCapture]()
	{
		if (SceneCapture && IsValid(SceneCapture))
		{
			SceneCapture->Destroy();
		}
	});
}
