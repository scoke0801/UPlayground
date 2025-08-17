// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIEnemyNamePlate.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UPGUIEnemyNamePlate::SetHpPercent(const float Percent)
{
	HpBar->SetPercent(Percent);
}

void UPGUIEnemyNamePlate::SetNameText(const FString& Name)
{
	NameTextBlock->SetText(FText::FromString(Name));
}
