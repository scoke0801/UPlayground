// Fill out your copyright notice in the Description page of Project Settings.


#include "PGEnemySkillHandler.h"

#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

void FPGEnemySkillHandler::UseSkill(const EPGSkillSlot InSlotId)
{
	// 부모 클래스의 UseSkill 호출 (쿨타임 업데이트)
	Super::UseSkill(InSlotId);

    // Keep authored weights stable. Repeated use must not snowball into higher priority.
}

bool FPGEnemySkillHandler::ResolveSkillSlot(int32 RequestedSkillID, const FGameplayTagContainer& Tags, EPGSkillSlot& OutSlot)
{
    if (SkillDataMap.IsEmpty()) return false;
    if (RequestedSkillID > 0)
    {
        OutSlot = FindSlotBySkillID(RequestedSkillID);
        return GetSkillID(OutSlot) == RequestedSkillID && IsCanUseSkill(OutSlot);
    }
    if (Tags.IsEmpty())
    {
        TArray<EPGSkillSlot> Ready;
        for (const auto& Pair : SkillDataMap) if (!Pair.Value.IsOnCooldown()) Ready.Add(Pair.Key);
        if (Ready.IsEmpty()) return false;
        OutSlot = Ready[FMath::RandHelper(Ready.Num())];
        return true;
    }
    // Legacy tag-only callers may choose any ready skill matching that type.
    OutSlot = GetSkillSlotByTag(Tags);
    return IsCanUseSkill(OutSlot);
}

EPGSkillSlot FPGEnemySkillHandler::GetSkillSlotByTag(const FGameplayTagContainer& GameplayTags)
{
	for (const FGameplayTag& Tag : GameplayTags)
	{
		if (Tag.MatchesTagExact(PGGamePlayTags::Enemy_Ability_MeleeSkill))
		{
			return GetRandomSkillSlotBySkillType(EPGSkillType::Melee);
		}
		else if (Tag.MatchesTagExact(PGGamePlayTags::Enemy_Ability_ProjectileSkill))
		{
			return GetRandomSkillSlotBySkillType(EPGSkillType::Projectile);
		}
		else if (Tag.MatchesTagExact(PGGamePlayTags::Enemy_Ability_AOESkill))
		{
			return GetRandomSkillSlotBySkillType(EPGSkillType::AreaOfEffect);
		}
		else if (Tag.MatchesTagExact(PGGamePlayTags::Enemy_Ability_SummonSkill))
		{
			return GetRandomSkillSlotBySkillType(EPGSkillType::SummonEnemy);
		}
		else if (Tag.MatchesTagExact(PGGamePlayTags::Enemy_Ability_HealSkill))
		{
			return GetRandomSkillSlotBySkillType(EPGSkillType::Heal);
		}
	}
	return GetRandomSkillSlot();
}

EPGSkillSlot FPGEnemySkillHandler::GetRandomSkillSlotBySkillType(const EPGSkillType InSkillType) const
{
	TArray<EPGSkillSlot> FilteredKeyList;

	for (const auto& Pair : SkillDataMap)
	{
		if (InSkillType == Pair.Value.SkillType && !Pair.Value.IsOnCooldown())
		{
			FilteredKeyList.Add(Pair.Key);
		}
	}

	if (0 == FilteredKeyList.Num())
	{
		return static_cast<EPGSkillSlot>(255); // No ready skill of this type; never substitute another type.
	}
	return FilteredKeyList[FMath::RandRange(0, FilteredKeyList.Num() - 1)];
}
