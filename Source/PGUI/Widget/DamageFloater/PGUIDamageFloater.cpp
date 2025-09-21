// Fill out your copyright notice in the Description page of Project Settings.


#include "PGUIDamageFloater.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Curves/CurveVector.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "Engine/World.h"
#include "PGUI/Manager/PGDamageFloaterManager.h"

void UPGUIDamageFloater::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ElapsedTime += InDeltaTime;
	bool IsDone = false;
	
	for (FPGDamageFloaterCurveData& CurveData : CurveDataList)
	{
		if (EPGDamageFloaterCurveType::Translation ==  CurveData.CurveType)
		{
			IsDone |= PlayTranslationAnimation(
				Cast<UCurveVector>(CurveData.Curve),
				InDeltaTime
			);
		}
		else if (EPGDamageFloaterCurveType::Scale ==  CurveData.CurveType)
		{
			IsDone |= PlayScaleAnimation(
			Cast<UCurveVector>(CurveData.Curve),
				InDeltaTime);
		}
		else if (EPGDamageFloaterCurveType::Opacity ==  CurveData.CurveType)
		{
			IsDone |= PlayOpacityAnimation(
			Cast<UCurveFloat>(CurveData.Curve),
				InDeltaTime);
		}
	}

	if (false == IsDone || ElapsedTime >= LifeTime)
	{
		PGDamageFloater()->ReturnFloaterToPool(DamageType, this);
	}
}

void UPGUIDamageFloater::SetDamage(float Damage, EPGDamageType InDamageType, FVector2D InBasePosition)
{
	DamageType = InDamageType;
	ElapsedTime = 0.0f;
	BasePosition = InBasePosition;
	
	if (DamageText)
	{
		// 데미지 값을 텍스트로 설정
		FText DamageDisplayText;
		
		if (Damage <= 0)
		{
			DamageDisplayText = FText::FromString("Miss!");
		}
		else
		{
			DamageDisplayText = FText::AsNumber(FMath::FloorToInt(Damage));
		}
		
		DamageText->SetText(DamageDisplayText);
		
		// 데미지 타입에 따른 색상 설정[ 임시... ]
		FLinearColor TextColor = FLinearColor::White;
		
		switch (DamageType)
		{
		case EPGDamageType::Normal:
			TextColor = FLinearColor::White;
			break;
		case EPGDamageType::Critical:
			TextColor = FLinearColor::Red;
			break;
		case EPGDamageType::Miss:
			TextColor = FLinearColor::Gray;
			break;
		case EPGDamageType::Heal:
			TextColor = FLinearColor::Green;
			break;
		}
		
		DamageText->SetColorAndOpacity(TextColor);
	}
}

FVector2D UPGUIDamageFloater::GetWidgetSize()
{
	if (false == bSizeCached)
	{
		// 최초 한 번만 계산
		ForceLayoutPrepass();
		CachedSize = GetDesiredSize();
		bSizeCached = true;
	}
	return CachedSize;
}

bool UPGUIDamageFloater::PlayTranslationAnimation(UCurveVector* Curve, float DeltaTime)
{
	float MinTime, MaxTime;
	Curve->GetTimeRange(MinTime, MaxTime);

	if (MaxTime > ElapsedTime)
	{
		FVector Result = Curve->GetVectorValue(ElapsedTime);

		SetRenderTranslation(FVector2D(Result));

		return true;
	}
	return false;
}

bool UPGUIDamageFloater::PlayScaleAnimation(UCurveVector* Curve, float DeltaTime)
{
	float MinTime, MaxTime;
	Curve->GetTimeRange(MinTime, MaxTime);

	if (MaxTime > ElapsedTime)
	{
		FVector Result = Curve->GetVectorValue(ElapsedTime);

		SetRenderScale(FVector2D(Result));
		return true;
	}
	
	return false;
}

bool UPGUIDamageFloater::PlayOpacityAnimation(UCurveFloat* Curve, float DeltaTime)
{
	float MinTime, MaxTime;
	Curve->GetTimeRange(MinTime, MaxTime);

	if (MaxTime > ElapsedTime)
	{
		float Result = Curve->GetFloatValue(ElapsedTime);

		SetRenderOpacity(Result);
		
		return true;
	}
	return false;
}

