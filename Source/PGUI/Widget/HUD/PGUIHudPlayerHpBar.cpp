// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIHudPlayerHpBar.h"

#include "Components/ProgressBar.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGShared/Shared/Message/Stat/PGStatUpdateEventData.h"

void UPGUIHudPlayerHpBar::NativeConstruct()
{
	Super::NativeOnInitialized();

	// Player 정보 가져와야 한다...
	PGMessage()->RegisterDelegate(EPGUIMessageType::StatUpdate,
		this, &ThisClass::OnStatUpdate);
}

void UPGUIHudPlayerHpBar::OnStatUpdate(const IPGEventData* InEventData)
{
	const FPGStatUpdateEventData* EventData = static_cast<const FPGStatUpdateEventData*>(InEventData);
	if (nullptr == EventData || EPGStatType::Hp != EventData->StatType)
	{
		return;
	}

	float Percent = static_cast<float>(EventData->Current) / EventData->Max;
	HpBar->SetPercent(Percent);
}
