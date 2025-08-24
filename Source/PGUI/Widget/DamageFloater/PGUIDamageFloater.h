// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGShared/Shared/Structure/PGDamageFloaterCurveData.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGUIDamageFloater.generated.h"

enum class EPGDamageType : uint8;
class UTextBlock;
/**
 * 
 */
UCLASS()
class PGUI_API UPGUIDamageFloater : public UPGWidgetBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI")
	TArray<FPGDamageFloaterCurveData> CurveDataList;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI", meta=(BindWidget))
	UTextBlock* DamageText;

public:
	void SetDamage(float Damage, EPGDamageType DamageType);
};
