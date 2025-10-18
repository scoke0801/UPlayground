// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGActor/Components/PGPawnExtensionComponentBase.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGStatComponent.generated.h"

enum class EPGDamageType : uint8;
/**
 * 
 */
UCLASS()
class PGACTOR_API UPGStatComponent : public UPGPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	TMap<EPGStatType, int32> StatMap;

	int32 CurrentHealth;
	
public:
	virtual void InitData(int32 CharacterTID);

	int32 GetStat(EPGStatType StatType) const;

public:
	int32 CalculateDamage(const UPGStatComponent* const OtherStatComponent, OUT EPGDamageType OutDamageType) const;
	int32 CalculateHealAmount() const;
};
