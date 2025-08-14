// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUISkillSlot.h"

#include "Components/Image.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

void UPGUISkillSlot::SetData(const PGSkillId InSkillId)
{
	if (FPGSkillDataRow* Data = UPGDataTableManager::Get()->GetRowData<FPGSkillDataRow>(InSkillId))
	{
		if (UObject* LoadedObject = Data->SkillIconPath.TryLoad())
		{
			if (UTexture2D* Texture = Cast<UTexture2D>(LoadedObject))
			{
				SkillImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				SkillImage->SetBrushFromTexture(Texture);
				return;
			}
		}
	}

	SkillImage->SetVisibility(ESlateVisibility::Collapsed);
}
