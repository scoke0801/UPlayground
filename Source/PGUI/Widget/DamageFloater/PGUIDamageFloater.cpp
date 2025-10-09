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

	// 수직 오프셋 부드럽게 보간
	if (!FMath::IsNearlyEqual(AdditionalVerticalOffset, TargetVerticalOffset, 0.1f))
	{
		AdditionalVerticalOffset = FMath::FInterpTo(
			AdditionalVerticalOffset,
			TargetVerticalOffset,
			InDeltaTime,
			OffsetInterpSpeed
		);
	}

	// 액터 추적 (액터가 설정되어 있고 유효한 경우)
	if (TargetActor.IsValid())
	{
		UpdateScreenPosition();
	}

	ElapsedTime += InDeltaTime;
	bool IsDone = false;
	
	for (FPGDamageFloaterCurveData& CurveData : CurveDataList)
	{
		if (EPGDamageFloaterCurveType::Scale ==  CurveData.CurveType)
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

	// 액터가 제거되었거나 수명이 다한 경우
	if ((TargetActor.IsValid() == false && !TargetActor.IsExplicitlyNull()) || 
		false == IsDone || 
		ElapsedTime >= LifeTime)
	{
		PGDamageFloater()->ReturnFloaterToPool(DamageType, this);
	}
}

void UPGUIDamageFloater::SetDamage(float Damage, EPGDamageType InDamageType, FVector2D InBasePosition)
{
	DamageType = InDamageType;
	ElapsedTime = 0.0f;
	BasePosition = InBasePosition;
	
	// 크기 캐시 리셋 (텍스트가 변경되므로)
	bSizeCached = false;
	
	// 추가 오프셋 초기화
	AdditionalVerticalOffset = 0.0f;
	TargetVerticalOffset = 0.0f;
	
	// 스택 인덱스 초기화 (최신 플로터)
	StackIndex = 0;
	
	// 투명도 초기화 (재사용 시 페이드아웃된 상태 초기화)
	SetRenderOpacity(1.0f);
	
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
		
		// 데미지 타입에 따른 색상 설정 (알파값은 1.0으로 초기화)
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
		
		// 알파값 명시적으로 1.0 설정 (새 플로터는 완전 불투명)
		TextColor.A = 1.0f;
		DamageText->SetColorAndOpacity(TextColor);
	}
}

void UPGUIDamageFloater::SetTargetActor(AActor* InTargetActor, FVector InLocalOffset)
{
	TargetActor = InTargetActor;
	LocalOffset = InLocalOffset;
}

void UPGUIDamageFloater::AddVerticalOffset(float Offset)
{
	TargetVerticalOffset += Offset;
}

void UPGUIDamageFloater::SetStackIndex(int32 InStackIndex, float InFadeOutStrength, float InMinOpacity)
{
	StackIndex = InStackIndex;
	FadeOutStrength = InFadeOutStrength;
	MinOpacity = InMinOpacity;
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

void UPGUIDamageFloater::UpdateScreenPosition()
{
	if (!TargetActor.IsValid())
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	
	// 액터의 현재 월드 위치 + 오프셋 계산
	FVector WorldLocation = TargetActor->GetActorLocation() + LocalOffset;
	
	// 월드 좌표 → 스크린 좌표 변환
	FVector2D ScreenPosition;
	if (PC->ProjectWorldLocationToScreen(WorldLocation, ScreenPosition))
	{
		// 위젯 중앙 정렬
		FVector2D WidgetSize = GetWidgetSize();
		ScreenPosition.X -= WidgetSize.X * 0.5f;
		
		// 추가 수직 오프셋 적용 (다른 플로터와 겹치지 않도록)
		ScreenPosition.Y -= AdditionalVerticalOffset;
		
		// 화면에 표시
		SetPositionInViewport(ScreenPosition);
	}
	else
	{
		// 화면 밖인 경우 숨김
		SetVisibility(ESlateVisibility::Hidden);
	}
}

bool UPGUIDamageFloater::PlayScaleAnimation(UCurveVector* Curve, float DeltaTime)
{
	if (!Curve)
	{
		return false;
	}
	
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
	if (!Curve)
	{
		return false;
	}
	
	float MinTime, MaxTime;
	Curve->GetTimeRange(MinTime, MaxTime);

	if (MaxTime > ElapsedTime)
	{
		float CurveOpacity = Curve->GetFloatValue(ElapsedTime);
		
		// 스택 인덱스에 따른 투명도 계산
		float StackOpacity = FMath::Clamp(
			1.0f - (StackIndex * FadeOutStrength),
			MinOpacity,
			1.0f
		);
		
		// 커브 투명도와 스택 투명도를 곱해서 적용
		SetRenderOpacity(CurveOpacity * StackOpacity);
		return true;
	}
	return false;
}
