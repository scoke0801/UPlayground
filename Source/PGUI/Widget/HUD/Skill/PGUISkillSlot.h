// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGShared/Shared/Define/PGSkillDefine.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGUISkillSlot.generated.h"

class UImage;
class UTextBlock;
/**
 * 
 */
UCLASS(meta = (DisableNativeTick))
class PGUI_API UPGUISkillSlot : public UPGWidgetBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	TObjectPtr<UImage> SkillImage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	TObjectPtr<UTextBlock> CoolTimeText;
	
protected:
	PGSkillId SkillId;

public:
	void SetData(const PGSkillId InSkillId);
};
