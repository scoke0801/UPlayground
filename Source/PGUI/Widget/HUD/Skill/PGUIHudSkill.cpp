// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIHudSkill.h"

#include "PGUISkillSlot.h"

constexpr int32 FIRST_SKILL_SLOT = 1;
constexpr int32 LAST_SKILL_SLOT = 6;

void UPGUIHudSkill::NativeOnInitialized()
{
	Super::NativeOnInitialized();

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

	int32 breakPoint =32;
	
}
