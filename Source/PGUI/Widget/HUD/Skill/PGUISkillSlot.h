// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGShared/Shared/Define/PGSkillDefine.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGUISkillSlot.generated.h"

class UImage;
class UTextBlock;
class UWidgetSwitcher;
/**
 * 
 */
UCLASS(meta = (DisableNativeTick))
class PGUI_API UPGUISkillSlot : public UPGWidgetBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	TObjectPtr<UImage> SkillImage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	TObjectPtr<UTextBlock> CoolTimeText;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|UI", meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> FrameImageSwitcher;
	
protected:
	PGSkillId SkillId;

protected:
	virtual void NativeConstruct() override;
	
public:
	void SetData(const PGSkillId InSkillId);
	
};
