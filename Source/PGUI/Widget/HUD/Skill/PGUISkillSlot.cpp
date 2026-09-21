// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUISkillSlot.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"

#include "Components/Button.h"
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
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"
#include "PGUI/Component/Base/PGButton.h"

constexpr int32 NORMAL_FRAME_INDEX = 0;
constexpr int32 LOCKED_FRAME_INDEX = 1;

void UPGUISkillSlot::NativeConstruct()
{
	Super::NativeConstruct();

	DelegateHandle = UPGMessageManager::Get(this)->RegisterDelegate(EPGPlayerMessageType::UseSkill, this,
		&ThisClass::OnPlayerUseSkill);

	SkillButton->OnClicked.AddDynamic(this, &ThisClass::OnButtonClicked);
}

void UPGUISkillSlot::NativeDestruct()
{
	SkillButton->OnClicked.RemoveDynamic(this,&ThisClass::OnButtonClicked);
	UPGMessageManager::Get(this)->UnregisterDelegate(EPGPlayerMessageType::UseSkill,DelegateHandle);

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
	CacheSkillTag(SkillSlot);
    SetCoolTime();
	
	if (FPGSkillDataRow* Data = UPGDataTableManager::Get(this)->GetRowData<FPGSkillDataRow>(InSkillId))
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

void UPGUISkillSlot::OnButtonClicked()
{
	FPGEventDataOneParam<FGameplayTag> ToSendData(SkillTag);
	UPGMessageManager::Get(this)->SendMessage(EPGUIMessageType::ClickSkillButton, &ToSendData);
}

void UPGUISkillSlot::SetCoolTime()
{
    UpdateCoolTime();
    if (RemainCoolTime > 0.f) GetWorld()->GetTimerManager().SetTimer(CoolTimeTimerHandle, this, &ThisClass::UpdateCoolTime, FMath::Max(.05f, CoolTimeTick), true);
}

void UPGUISkillSlot::OnPlayerUseSkill(const class IPGEventData* InData)
{
	const FPGEventDataTwoParam<PGSkillId, EPGSkillSlot>* CastedParam =
		static_cast<const FPGEventDataTwoParam<PGSkillId, EPGSkillSlot>*>(InData);

	if (nullptr == CastedParam || CastedParam->ValueB != SkillSlot)
	{
		return;
	}

	SetCoolTime();
	SkillButton->PlayClickEffect();
}

void UPGUISkillSlot::UpdateCoolTime()
{
    RemainCoolTime = 0.f;
    const auto* Character = Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
    if (Character && Character->GetSkillHandler())
        if (const auto* Data = Character->GetSkillHandler()->GetSkillData(SkillSlot)) RemainCoolTime = Data->GetRemainingCooldown();
    if (!CoolTimeText) return;
    CoolTimeText->SetVisibility(RemainCoolTime > 0.f ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    CoolTimeText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), RemainCoolTime)));
    if (RemainCoolTime <= 0.f && GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CoolTimeTimerHandle);
}

void UPGUISkillSlot::CacheSkillTag(const EPGSkillSlot InSkillSlot)
{
	switch (InSkillSlot)
	{
	case EPGSkillSlot::NormalAttack: SkillTag = PGGamePlayTags::InputTag_Skill_Normal; break;
	case EPGSkillSlot::SkillSlot_1:	SkillTag = PGGamePlayTags::InputTag_Skill_Slot1; break;
	case EPGSkillSlot::SkillSlot_2:	SkillTag = PGGamePlayTags::InputTag_Skill_Slot2; break;
	case EPGSkillSlot::SkillSlot_3:	SkillTag = PGGamePlayTags::InputTag_Skill_Slot3; break;
	case EPGSkillSlot::SkillSlot_4:	SkillTag = PGGamePlayTags::InputTag_Skill_Slot4; break;
	case EPGSkillSlot::SkillSlot_5:	SkillTag = PGGamePlayTags::InputTag_Skill_Slot5; break;
	case EPGSkillSlot::SkillSlot_6:	SkillTag = PGGamePlayTags::InputTag_Skill_Slot6; break;
		default: break;
	}
}
