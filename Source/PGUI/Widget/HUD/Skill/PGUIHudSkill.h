// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGUIHudSkill.generated.h"

class UPGUISkillSlot;
/**
 * 
 */
UCLASS()
class PGUI_API UPGUIHudSkill : public UPGWidgetBase
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TArray<TObjectPtr<UPGUISkillSlot>> SkillSlots;

protected:
	virtual void NativeOnInitialized() override;
	
};
