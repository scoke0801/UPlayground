// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIHudPlayerInfo.h"

#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
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

	// Scene Capture Actor 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	APGSceneCapture* SceneCapture = World->SpawnActor<APGSceneCapture>(APGSceneCapture::StaticClass(), SpawnParams);
	if (!SceneCapture)
	{
		return;
	}

	// Render Target 생성 및 초기화
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(SceneCapture);
	if (RenderTarget)
	{
		RenderTarget->InitAutoFormat(256, 256);
		RenderTarget->ClearColor = FLinearColor::Transparent;
		RenderTarget->bAutoGenerateMips = false;
		
		SceneCapture->SetRenderTarget(RenderTarget);

		// 캡처 실행
		SceneCapture->CaptureCurrentScene(PlayerCharacter);

		// UI에 텍스처 설정 (RenderTarget은 UTexture를 상속받으므로 SetBrushResourceObject 사용)
		PlayerImage->SetBrushResourceObject(RenderTarget);
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
