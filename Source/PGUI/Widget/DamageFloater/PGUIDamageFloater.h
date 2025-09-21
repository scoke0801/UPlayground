// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveVector.h"
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PG|UI")
	float LifeTime = 1.5f;

private:
	FVector2D CachedSize = FVector2D::ZeroVector;
	bool bSizeCached = false;
	
private:
	float ElapsedTime = 0.0f;
	FVector2D BasePosition;
	EPGDamageType DamageType;
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
public:
	void SetDamage(float Damage, EPGDamageType DamageType, FVector2D InBasePosition);

	FVector2D GetWidgetSize();
	
private:
	bool PlayTranslationAnimation(UCurveVector* Curve,float DeltaTime);
	bool PlayScaleAnimation(UCurveVector* Curve,float DeltaTime);
	bool PlayOpacityAnimation(UCurveFloat* Curve,float DeltaTime);
};
