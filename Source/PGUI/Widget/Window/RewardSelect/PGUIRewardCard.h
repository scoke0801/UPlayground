// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGUIRewardCard.generated.h"

class UTextBlock;
class UWidgetSwitcher;
class UImage;

enum class EPGRewardGrade : uint8;
/**
 * 
 */
UCLASS()
class PGUI_API UPGUIRewardCard : public UPGWidgetBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	UWidgetSwitcher* GradeBGSwitcher;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	UTextBlock* RewardNameText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	UWidgetSwitcher* RewardTypeSwitcher;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	UImage* ItemImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	UTextBlock* ItemDescText;

protected:	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	UTextBlock* StatDescText;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	UImage* SkillImage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	UTextBlock* SkillDescText;

public:
	void SetGrade(const EPGRewardGrade InGrade);

};
