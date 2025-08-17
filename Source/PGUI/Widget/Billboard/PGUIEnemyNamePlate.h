// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGUIPlayerHpBar.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGUIEnemyNamePlate.generated.h"

/**
 * 
 */
UCLASS()
class PGUI_API UPGUIEnemyNamePlate : public UPGWidgetBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	class UProgressBar* HpBar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	class UTextBlock* NameTextBlock;

private:
	UPROPERTY()
	FTimerHandle HideTimerHandle;

protected:
	virtual void NativeDestruct() override;
	
public:
	void SetHpPercent(const float Percent);
	void SetNameText(const FString& Name);

	void ShowWidget(float InLifeTime);

private:
	void HideWidget();
};
