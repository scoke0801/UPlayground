// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUISkillSlot.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"

constexpr int32 NORMAL_FRAME_INDEX = 0;
constexpr int32 LOCKED_FRAME_INDEX = 1;

void UPGUISkillSlot::NativeConstruct()
{
	Super::NativeConstruct();

	FrameImageSwitcher->SetActiveWidgetIndex(LOCKED_FRAME_INDEX);
	SkillImage->SetVisibility(ESlateVisibility::Collapsed);
	CoolTimeText->SetVisibility(ESlateVisibility::Collapsed);
}

void UPGUISkillSlot::SetData(const PGSkillId InSkillId)
{
	if (FPGSkillDataRow* Data = UPGDataTableManager::Get()->GetRowData<FPGSkillDataRow>(InSkillId))
	{
		if (UObject* LoadedObject = Data->SkillIconPath.TryLoad())
		{
			if (UTexture2D* Texture = Cast<UTexture2D>(LoadedObject))
			{
				FrameImageSwitcher->SetActiveWidgetIndex(NORMAL_FRAME_INDEX);
				SkillImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				SkillImage->SetBrushFromTexture(Texture);
				return;
			}
		}
	}

	FrameImageSwitcher->SetActiveWidgetIndex(LOCKED_FRAME_INDEX);
	SkillImage->SetVisibility(ESlateVisibility::Collapsed);
}
