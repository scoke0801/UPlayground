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
	int32 CalculateDamage(const UPGStatComponent* const OtherStatComponent, OUT EPGDamageType& OutDamageType) const;
	
	// 무기를 포함한 데미지 계산
	int32 CalculateDamageWithWeapon(const UPGStatComponent* const OtherStatComponent, 
		const class UPGPawnCombatComponent* const OtherCombatComponent, OUT EPGDamageType& OutDamageType) const;
	
	// 무기 유무를 자동 판별하여 데미지 계산
	int32 CalculateDamageAuto(const UPGStatComponent* const OtherStatComponent, 
		const class UPGPawnCombatComponent* const OtherCombatComponent, OUT EPGDamageType& OutDamageType) const;
		
	// 총 공격력 계산 (캐릭터 + 무기)
	int32 GetTotalAttackPower(const class UPGPawnCombatComponent* CombatComponent) const;
		
	int32 CalculateHealAmount() const;
};
