// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGUIHudPlayerInfo.generated.h"

/**
 * 
 */
UCLASS(meta = (DisableNativeTick))
class PGUI_API UPGUIHudPlayerInfo : public UPGWidgetBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	class UTextBlock* LevelText;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	class UPGWidgetBase* HpBar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	class UImage* PlayerImage;

protected:
	virtual void NativeOnInitialized() override;

private:
	void OnStatUpdate(const IPGEventData* InEventData);
};
