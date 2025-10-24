// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIPlayerHpBar.h"

#include "Components/ProgressBar.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGShared/Shared/Message/Stat/PGStatUpdateEventData.h"

void UPGUIPlayerHpBar::NativeConstruct()
{
	Super::NativeOnInitialized();

	// Player 정보 가져와야 한다...
	PGMessage()->RegisterDelegate(EPGPlayerMessageType::StatUpdate,
		this, &ThisClass::OnStatUpdate);
}

void UPGUIPlayerHpBar::SetHpPercent(const float Percent)
{
	HpBar->SetPercent(Percent);
}

void UPGUIPlayerHpBar::OnStatUpdate(const IPGEventData* InEventData)
{
	const FPGStatUpdateEventData* EventData = static_cast<const FPGStatUpdateEventData*>(InEventData);
	if (nullptr == EventData || EPGStatType::Health != EventData->StatType)
	{
		return;
	}

	float Percent = static_cast<float>(EventData->Current) / EventData->Max;
	SetHpPercent(Percent);
}
