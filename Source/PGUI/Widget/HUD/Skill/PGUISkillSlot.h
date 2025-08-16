// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGShared/Shared/Define/PGSkillDefine.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGUISkillSlot.generated.h"

struct FPGSkillDataRow;
enum class EPGSkillSlot : uint8;
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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI")
	float CoolTimeTick = 0.5f;
	
protected:
	PGSkillId SkillId;
	EPGSkillSlot SkillSlot;

	FPGSkillDataRow* CachedSkillData = nullptr;
	
	float RemainCoolTime = 0.0f;
	
	FTimerHandle CoolTimeTimerHandle;
	FDelegateHandle DelegateHandle;
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
public:
	void SetData(const EPGSkillSlot InSkillSlot, const PGSkillId InSkillId);

private:
	void SetCoolTime();
	void OnPlayerUseSkill(const class IPGEventData* InData);

	void UpdateCoolTime();
};