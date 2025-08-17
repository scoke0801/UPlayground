// Fill out your copyright notice in the Description page of Project Settings.


#include "PGPlayerStatComponent.h"

#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGShared/Shared/Message/Stat/PGStatUpdateEventData.h"

void UPGPlayerStatComponent::InitData(int32 CharacterTID)
{
	Super::InitData(CharacterTID);

	FPGStatUpdateEventData EventData(EPGStatType::Hp, CurrentHP, MaxHP);
	PGMessage()->SendMessage(EPGPlayerMessageType::StatUpdate, &EventData);
}
