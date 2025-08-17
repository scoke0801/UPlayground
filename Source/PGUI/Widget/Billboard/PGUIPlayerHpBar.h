// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGUIPlayerHpBar.generated.h"

class UProgressBar;
/**
 * 
 */
UCLASS(meta = (DisableNativeTick))
class PGUI_API UPGUIPlayerHpBar : public UPGWidgetBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	class UProgressBar* HpBar;
	
protected:
	virtual void NativeConstruct() override;

public:
	void SetHpPercent(const float Percent);
	
private:
	void OnStatUpdate(const IPGEventData* InEventData);
};
