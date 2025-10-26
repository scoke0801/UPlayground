// Fill out your copyright notice in the Description page of Project Settings.


#include "PGStatComponent.h"

#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Stat/PGCharacterStatDataRow.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "PGActor/Components/Combat/PGPawnCombatComponent.h"
#include "PGActor/Weapon/PGWeaponBase.h"

void UPGStatComponent::InitData(int32 CharacterTID)
{
	// 테이블에서 읽어오는 구조로 나중에 고치고 우선 상수

	if (UPGDataTableManager* Manager = PGData())
	{
		if (FPGCharacterStatDataRow* Data = Manager->GetRowData<FPGCharacterStatDataRow>(CharacterTID))
		{
			StatMap = Data->Stats;
		}
	}
	if (StatMap.Contains(EPGStatType::Health))
	{
		CurrentHealth = StatMap[EPGStatType::Health];
	}
}

int32 UPGStatComponent::GetStat(EPGStatType StatType) const
{
	if (StatMap.Contains(StatType))
	{
		return StatMap[StatType];
	}
	return 0;
}

int32 UPGStatComponent::CalculateDamage(const UPGStatComponent* const OtherStatComponent, OUT EPGDamageType& OutDamageType) const
{
	// 1. 기본 데미지
	float BaseDamage = OtherStatComponent->GetStat(EPGStatType::Attack);
    
	// 2. 방어력 적용 (방어력이 높을수록 데미지 감소)
	float DefenseReduction = GetStat(EPGStatType::Defense) / (GetStat(EPGStatType::Defense) + 100.0f);
	float DamageAfterDefense = BaseDamage * (1.0f - DefenseReduction);
    
	// 3. 치명타 판정 (0-10000 확률)
	bool bIsCritical = FMath::RandRange(0.0f, 10000.0f) <= OtherStatComponent->GetStat(EPGStatType::CriticalRate);

	OutDamageType = bIsCritical ? EPGDamageType::Critical : EPGDamageType::Normal;
	
	// 4. 최종 데미지 계산
	float FinalDamage = DamageAfterDefense;
	if (bIsCritical)
	{
		FinalDamage *= (1.5f);
	}
    
	// 5. 최소 데미지 보장 (1 이상)
	return FMath::Max(1.0f, FinalDamage);
}

int32 UPGStatComponent::CalculateDamageWithWeapon(const UPGStatComponent* const OtherStatComponent, 
	const UPGPawnCombatComponent* const OtherCombatComponent, OUT EPGDamageType& OutDamageType) const
{
	// 1. 기본 공격력 (캐릭터 스탯)
	float BaseDamage = OtherStatComponent->GetStat(EPGStatType::Attack);
	
	// 2. 무기 스탯 추가
	if (OtherCombatComponent)
	{
		if (APGWeaponBase* CurrentWeapon = OtherCombatComponent->GetCharacterCurrentEquippedWeapon())
		{
			// 무기의 공격력 추가
			BaseDamage += CurrentWeapon->GetWeaponStat(EPGStatType::Attack);
		}
	}
    
	// 3. 방어력 적용 (방어력이 높을수록 데미지 감소)
	float DefenseReduction = GetStat(EPGStatType::Defense) / (GetStat(EPGStatType::Defense) + 100.0f);
	float DamageAfterDefense = BaseDamage * (1.0f - DefenseReduction);
    
	// 4. 치명타 확률 계산 (캐릭터 + 무기)
	float CriticalRate = OtherStatComponent->GetStat(EPGStatType::CriticalRate);
	if (OtherCombatComponent)
	{
		if (APGWeaponBase* CurrentWeapon = OtherCombatComponent->GetCharacterCurrentEquippedWeapon())
		{
			CriticalRate += CurrentWeapon->GetWeaponStat(EPGStatType::CriticalRate);
		}
	}
	
	// 5. 치명타 판정 (0-10000 확률)
	bool bIsCritical = FMath::RandRange(0.0f, 10000.0f) <= CriticalRate;

	OutDamageType = bIsCritical ? EPGDamageType::Critical : EPGDamageType::Normal;
	
	// 6. 치명타 데미지 배율 계산 (캐릭터 + 무기)
	float CriticalMultiplier = 1.5f; // 기본 치명타 배율
	if (bIsCritical)
	{
		// 무기에 치명타 데미지 스탯이 있다면 추가 적용
		if (OtherCombatComponent)
		{
			if (APGWeaponBase* CurrentWeapon = OtherCombatComponent->GetCharacterCurrentEquippedWeapon())
			{
				float WeaponCritDamage = CurrentWeapon->GetWeaponStat(EPGStatType::CriticalDamage);
				if (WeaponCritDamage > 0)
				{
					CriticalMultiplier += (WeaponCritDamage / 100.0f); // 퍼센트로 계산
				}
			}
		}
	}
	
	// 7. 최종 데미지 계산
	float FinalDamage = DamageAfterDefense;
	if (bIsCritical)
	{
		FinalDamage *= CriticalMultiplier;
	}
    
	// 8. 최소 데미지 보장 (1 이상)
	return FMath::Max(1.0f, FinalDamage);
}

int32 UPGStatComponent::CalculateDamageAuto(const UPGStatComponent* const OtherStatComponent, 
	const UPGPawnCombatComponent* const OtherCombatComponent, OUT EPGDamageType& OutDamageType) const
{
	// 무기가 있으면 무기 포함 계산, 없으면 기본 계산
	if (OtherCombatComponent && OtherCombatComponent->GetCharacterCurrentEquippedWeapon())
	{
		return CalculateDamageWithWeapon(OtherStatComponent, OtherCombatComponent, OutDamageType);
	}
	else
	{
		return CalculateDamage(OtherStatComponent, OutDamageType);
	}
}

int32 UPGStatComponent::GetTotalAttackPower(const UPGPawnCombatComponent* CombatComponent) const
{
	int32 BaseAttack = GetStat(EPGStatType::Attack);
	
	if (CombatComponent)
	{
		if (APGWeaponBase* CurrentWeapon = CombatComponent->GetCharacterCurrentEquippedWeapon())
		{
			BaseAttack += CurrentWeapon->GetWeaponStat(EPGStatType::Attack);
		}
	}
	
	return BaseAttack;
}

int32 UPGStatComponent::CalculateHealAmount() const
{
	return GetStat(EPGStatType::HealAmount);
}
