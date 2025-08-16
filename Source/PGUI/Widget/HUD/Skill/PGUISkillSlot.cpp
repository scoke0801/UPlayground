// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUISkillSlot.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataTemplate.h"

constexpr int32 NORMAL_FRAME_INDEX = 0;
constexpr int32 LOCKED_FRAME_INDEX = 1;

void UPGUISkillSlot::NativeConstruct()
{
	Super::NativeConstruct();

	DelegateHandle = UPGMessageManager::Get()->RegisterDelegate(EPGPlayerMessageType::UseSkill, this,
		&ThisClass::OnPlayerUseSkill);
}

void UPGUISkillSlot::NativeDestruct()
{
	PGMessage()->UnregisterDelegate(EPGPlayerMessageType::UseSkill,DelegateHandle);

	if (CoolTimeTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(CoolTimeTimerHandle);
	}
	Super::NativeDestruct();
}

void UPGUISkillSlot::SetData(const EPGSkillSlot InSkillSlot, const PGSkillId InSkillId)
{
	SkillSlot = InSkillSlot;
	SkillId = InSkillId;
	
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

void UPGUISkillSlot::SetCoolTime()
{
	if (nullptr == CachedSkillData)
	{
		CachedSkillData = UPGDataTableManager::Get()->GetRowData<FPGSkillDataRow>(SkillId);
	}

	if (nullptr == CachedSkillData)
	{
		return;
	}

	if (0 < CachedSkillData->SkillCoolTime)
	{
		RemainCoolTime = CachedSkillData->SkillCoolTime;
		CoolTimeText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		CoolTimeText->SetText(FText::FromString(FString::FromInt(RemainCoolTime)));
		GetWorld()->GetTimerManager().SetTimer(CoolTimeTimerHandle, this, &UPGUISkillSlot::UpdateCoolTime, CoolTimeTick,true);
	}
}

void UPGUISkillSlot::OnPlayerUseSkill(const class IPGEventData* InData)
{
	const FPGEventDataTwoParam<PGSkillId, EPGSkillSlot>* CastedParam =
		static_cast<const FPGEventDataTwoParam<PGSkillId, EPGSkillSlot>*>(InData);

	if (nullptr == CastedParam ||
		CastedParam->ValueB != SkillSlot)
	{
		return;
	}

	SetCoolTime();
}

void UPGUISkillSlot::UpdateCoolTime()
{
	RemainCoolTime -= CoolTimeTick;
	if (FMath::IsNearlyEqual(0, RemainCoolTime))
	{
		CoolTimeText->SetVisibility(ESlateVisibility::Collapsed);
		GetWorld()->GetTimerManager().ClearTimer(CoolTimeTimerHandle);
	}

	CoolTimeText->SetText(FText::FromString(FString::FromInt(RemainCoolTime)));
}
