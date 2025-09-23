// Fill out your copyright notice in the Description page of Project Settings.


#include "PGEnemySkillHandler.h"

#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void FPGEnemySkillHandler::UseSkill(const EPGSkillSlot InSlotId)
{
	FPGSkillData* SkillData = GetSkillData(InSlotId);
	if (nullptr == SkillData)
	{
		return;
	}

	SkillData->Priority += 1;
}

EPGSkillSlot FPGEnemySkillHandler::GetSkillSlotByTag(const FGameplayTagContainer& GameplayTags)
{
	for (const FGameplayTag& Tag : GameplayTags)
	{
		if (Tag.MatchesTagExact(PGGamePlayTags::Enemy_Ability_MeleeSkill))
		{
			return GetRandomSkillSlotBySkillType(EPGSkillType::Melee);
		}
		else if (Tag.MatchesTagExact(PGGamePlayTags::Enemy_Ability_RangeSkill))
		{
			return GetRandomSkillSlotBySkillType(EPGSkillType::Range);
		}
		else if (Tag.MatchesTagExact(PGGamePlayTags::Enemy_Ability_SpawnSkill))
		{
			return GetRandomSkillSlotBySkillType(EPGSkillType::Heal);
		}
		else if (Tag.MatchesTagExact(PGGamePlayTags::Enemy_Ability_HealSkill))
		{
			return GetRandomSkillSlotBySkillType(EPGSkillType::SpawnEnemy);
		}
	}
	return GetRandomSkillSlot();
}

EPGSkillSlot FPGEnemySkillHandler::GetRandomSkillSlotBySkillType(const EPGSkillType InSkillType) const
{
	TArray<EPGSkillSlot> FilteredKeyList;

	for (const auto& Pair : SkillDataMap)
	{
		if (InSkillType == Pair.Value.SkillType)
		{
			FilteredKeyList.Add(Pair.Key);
		}
	}

	if (0 == FilteredKeyList.Num())
	{
		return EPGSkillSlot::NormalAttack;
	}
	return FilteredKeyList[FMath::RandRange(0, FilteredKeyList.Num() - 1)];
}