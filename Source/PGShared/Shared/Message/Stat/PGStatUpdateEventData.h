// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"

enum class EPGStatType : uint8;

class FPGStatUpdateEventData : public IPGEventData
{
public:
	EPGStatType StatType;
	
	int32 Current;
	int32 Max;

	FPGStatUpdateEventData(EPGStatType statType,
		int32 Current, int32 Max) : StatType(statType), Current(Current), Max(Max) 
	{}

	virtual ~FPGStatUpdateEventData() override = default;
	virtual FString ToString() const override { return TEXT("FPGStatUpdateEventData"); }	
};
