// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIHudPlayerInfo.h"

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
