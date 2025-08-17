// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGUIHudSkill.generated.h"

enum class EPGSkillSlot : uint8;
class UPGUISkillSlot;
/**
 * 
 */
UCLASS(meta = (DisableNativeTick))
class PGUI_API UPGUIHudSkill : public UPGWidgetBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|UI", meta = (BindWidget))
	TObjectPtr<UPGUISkillSlot> NormalAttackSlot;
	
	UPROPERTY()
	TArray<TObjectPtr<UPGUISkillSlot>> SkillSlots;

protected:
	virtual void NativeOnInitialized() override;

public:
	void SetSkillSlot(const EPGSkillSlot InSkillSlot) const;
	
private:
	void BindSkillSlots();

	TObjectPtr<UPGUISkillSlot> GetSkillSlot(const EPGSkillSlot) const;

private:
	void OnPlayerInit(const class IPGEventData* InData);
};
