// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGUIHudStage.generated.h"

/**
 * 
 */
UCLASS(meta = (DisableNativeTick))
class PGUI_API UPGUIHudStage : public UPGWidgetBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	class UTextBlock* StageText;
	
protected:
	virtual void NativeOnInitialized() override;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	FDelegateHandle StageChangeHandle;
	
private:
	void OnStageUpdated(const IPGEventData* InEventData);
};
