// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIEnemyNamePlate.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UPGUIEnemyNamePlate::NativeDestruct()
{
	if (HideTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(HideTimerHandle);
	}
	Super::NativeDestruct();
}

void UPGUIEnemyNamePlate::SetHpPercent(const float Percent)
{
	HpBar->SetPercent(Percent);
}

void UPGUIEnemyNamePlate::SetNameText(const FString& Name)
{
	NameTextBlock->SetText(FText::FromString(Name));
}

void UPGUIEnemyNamePlate::ShowWidget(float InLifeTime)
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	
	if (false == FMath::IsNearlyEqual(InLifeTime, 0.0f))
	{
		GetWorld()->GetTimerManager().SetTimer(HideTimerHandle,
			this, &ThisClass::HideWidget, InLifeTime);
	}
}

void UPGUIEnemyNamePlate::HideWidget()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
