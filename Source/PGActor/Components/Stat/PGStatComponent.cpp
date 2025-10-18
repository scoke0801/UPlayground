// Fill out your copyright notice in the Description page of Project Settings.


#include "PGStatComponent.h"

#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Stat/PGCharacterStatDataRow.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"

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

int32 UPGStatComponent::CalculateDamage(const UPGStatComponent* const OtherStatComponent, OUT EPGDamageType OutDamageType) const
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

int32 UPGStatComponent::CalculateHealAmount() const
{
	return GetStat(EPGStatType::HealAmount);
}
