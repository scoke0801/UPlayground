// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIHudStage.h"

#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PGActor/Util/PGSceneCapture.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataTemplate.h"

void UPGUIHudStage::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	if (UPGMessageManager* MessageManager = PGMessage())
	{
		StageChangeHandle = MessageManager->RegisterDelegate(EPGUIMessageType::StageChanged,
		this, &ThisClass::OnStageUpdated);
	}
}

void UPGUIHudStage::NativeConstruct()
{
	Super::NativeConstruct();
}

void UPGUIHudStage::NativeDestruct()
{
	Super::NativeDestruct();

	if (UPGMessageManager* MessageManager = PGMessage())
	{
		MessageManager->UnregisterDelegate(EPGUIMessageType::StageChanged, StageChangeHandle);
	}
}

void UPGUIHudStage::OnStageUpdated(const IPGEventData* InEventData)
{
	const FPGEventDataOneParam<int32>* EventData = static_cast<const FPGEventDataOneParam<int32>*>(InEventData);
	if (nullptr == EventData)
	{
		return;
	}

	FString StageString = FString::Printf(TEXT("Stage %d"), EventData->Value);
	StageText->SetText(FText::FromString(StageString));
}