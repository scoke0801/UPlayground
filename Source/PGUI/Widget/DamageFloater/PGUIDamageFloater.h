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
	
	float ElapsedTime = 0.0f;
	FVector2D BasePosition;
	EPGDamageType DamageType;
	
	// 액터 추적용
	TWeakObjectPtr<AActor> TargetActor;
	FVector LocalOffset;
	
	// 추가 수직 오프셋 (다른 플로터와 겹치지 않도록)
	float AdditionalVerticalOffset = 0.0f;
	float TargetVerticalOffset = 0.0f;
	
	// 스택에서의 위치 (0 = 최신, 1, 2, 3... = 오래된 순서)
	int32 StackIndex = 0;
	float FadeOutStrength = 0.15f;
	float MinOpacity = 0.3f;
	
	// 오프셋 보간 속도
	UPROPERTY(EditDefaultsOnly, Category = "PG|Animation")
	float OffsetInterpSpeed = 10.0f;
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
public:
	void SetDamage(float Damage, EPGDamageType DamageType, FVector2D InBasePosition);
	void SetTargetActor(AActor* InTargetActor, FVector InLocalOffset);
	void AddVerticalOffset(float Offset);
	void SetStackIndex(int32 InStackIndex, float InFadeOutStrength, float InMinOpacity);
	
	AActor* GetTargetActor() const { return TargetActor.Get(); }
	FVector2D GetWidgetSize();
	
private:
	void UpdateScreenPosition();
	bool PlayTranslationAnimation(UCurveVector* Curve,float DeltaTime);
	bool PlayScaleAnimation(UCurveVector* Curve,float DeltaTime);
	bool PlayOpacityAnimation(UCurveFloat* Curve,float DeltaTime);
};
