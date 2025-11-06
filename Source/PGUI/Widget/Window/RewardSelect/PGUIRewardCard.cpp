// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIRewardCard.h"

#include "Components/WidgetSwitcher.h"
#include "PGShared/Shared/Enum/PGRewardTypes.h"

void UPGUIRewardCard::SetGrade(const EPGRewardGrade InGrade)
{
	switch (InGrade)
	{
	case EPGRewardGrade::Normal: GradeBGSwitcher->SetActiveWidgetIndex(0); break;
	case EPGRewardGrade::Magic: GradeBGSwitcher->SetActiveWidgetIndex(1); break;
	case EPGRewardGrade::Rare: GradeBGSwitcher->SetActiveWidgetIndex(2); break;
	default:break;
	}
}
