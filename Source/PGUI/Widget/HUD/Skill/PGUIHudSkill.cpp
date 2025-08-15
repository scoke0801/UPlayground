// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIHudSkill.h"

#include "PGUISkillSlot.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"

constexpr int32 FIRST_SKILL_SLOT = 1;
constexpr int32 LAST_SKILL_SLOT = 6;

void UPGUIHudSkill::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	BindSkillSlots();
}

void UPGUIHudSkill::SetSkillSlot(const EPGSkillSlot InSkillSlot) const
{
	APGCharacterPlayer* Player = Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
	if (nullptr == Player)
	{
		return;
	}

	FPGSkillHandler* SkillHandler = Player->GetSkillHandler();
	if (nullptr == SkillHandler)
	{
		return;
	}

	if (UPGUISkillSlot* SkillSlot = GetSkillSlot(InSkillSlot))
	{
		SkillSlot->SetData(SkillHandler->GetSkillID(InSkillSlot));
	}
}

void UPGUIHudSkill::BindSkillSlots()
{
	FString SkillSlotName = TEXT("SkillSlot");
	for (int i = FIRST_SKILL_SLOT; i <= LAST_SKILL_SLOT; ++i)
	{
		FString SlotName = SkillSlotName + FString::FromInt(i);
		if (UPGUISkillSlot* SkillSlot = Cast<UPGUISkillSlot>(GetWidgetFromName(FName(*SlotName))))
		{
			SkillSlot->SetData(i);
			SkillSlots.Add(SkillSlot);
		}
	}
}

TObjectPtr<UPGUISkillSlot> UPGUIHudSkill::GetSkillSlot(const EPGSkillSlot InSkillSlot) const
{
	switch (InSkillSlot)
	{
	case EPGSkillSlot::NormalAttack:
		return NormalAttackSlot;
	case EPGSkillSlot::SkillSlot_1:
	case EPGSkillSlot::SkillSlot_2:
	case EPGSkillSlot::SkillSlot_3:
	case EPGSkillSlot::SkillSlot_4:
	case EPGSkillSlot::SkillSlot_5:
	case EPGSkillSlot::SkillSlot_6:
		{
			int32 Index = StaticCast<int32>(InSkillSlot) - StaticCast<int32>(EPGSkillSlot::SkillSlot_1);
		
			if (SkillSlots.IsValidIndex(Index))
			{
				return SkillSlots[Index];
			}
		}
		break;
	default: break;
	}
	return nullptr;
}
