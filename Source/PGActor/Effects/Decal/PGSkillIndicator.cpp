// Fill out your copyright notice in the Description page of Project Settings.


#include "PGSkillIndicator.h"

#include "Components/DecalComponent.h"

APGSkillIndicator::APGSkillIndicator()
{
	PrimaryActorTick.bCanEverTick = false;
}

void APGSkillIndicator::BeginPlay()
{
	Super::BeginPlay();

	UDecalComponent* DecalComponent = GetDecal();
	if (!DecalComponent)
	{
		return;
	}

	// DecalMaterial이 설정되어 있으면 Dynamic Material Instance 생성
	if (DecalMaterial)
	{
		DynamicMaterialInstance = UMaterialInstanceDynamic::Create(DecalMaterial, this);
		DecalComponent->SetDecalMaterial(DynamicMaterialInstance);
	}
	else
	{
		// DecalMaterial이 없으면 기존 머티리얼로 Dynamic Instance 생성 시도
		UMaterialInterface* ExistingMaterial = DecalComponent->GetDecalMaterial();
		if (ExistingMaterial)
		{
			DynamicMaterialInstance = UMaterialInstanceDynamic::Create(ExistingMaterial, this);
			DecalComponent->SetDecalMaterial(DynamicMaterialInstance);
		}
		else
		{
			return;
		}
	}

	// 자동 시작 설정이 되어있으면 애니메이션 시작
	if (bAutoStartAnimation)
	{
		StartAnimation();
	}
}

void APGSkillIndicator::BeginDestroy()
{
	EndAnimation();
	
	Super::BeginDestroy();
}

void APGSkillIndicator::StartAnimation()
{
	if (!DynamicMaterialInstance)
	{
		return;
	}

	// 이미 실행 중이면 정지
	if (bIsAnimating)
	{
		EndAnimation();
	}

	// 초기값 설정
	CurrentTime = 0.0f;
	bIsAnimating = true;

	// 초기 FillProgress 값 설정 (-1.0)
	DynamicMaterialInstance->SetScalarParameterValue(TEXT("FillProgress"), -1.0f);

	// 타이머 시작 (60FPS로 업데이트)
	GetWorldTimerManager().SetTimer(
		FillTimerHandle,
		this,
		&ThisClass::UpdateFillProgress,
		1.0f / 60.0f, // 매 프레임
		true          // 반복
	);
}

void APGSkillIndicator::EndAnimation()
{
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		if (FillTimerHandle.IsValid())
		{
			World->GetTimerManager().ClearTimer(FillTimerHandle);
		}
		if (DestroyTimerHandle.IsValid())
		{
			World->GetTimerManager().ClearTimer(DestroyTimerHandle);
		}
	}
	
	bIsAnimating = false;
}

float APGSkillIndicator::GetAnimationProgress() const
{
	if (AnimationDuration <= 0.0f)
		return 0.0f;
        
	return FMath::Clamp(CurrentTime / AnimationDuration, 0.0f, 1.0f);
}

void APGSkillIndicator::UpdateFillProgress()
{
	if (!bIsAnimating || !DynamicMaterialInstance)
		return;

	// 시간 증가
	CurrentTime += 1.0f / 60.0f;

	// Alpha 계산 (0.0 ~ 1.0)
	float Alpha = FMath::Clamp(CurrentTime / AnimationDuration, 0.0f, 1.0f);

	// FillProgress 값 계산 (-1.0 ~ 1.0)
	float FillValue = CalculateFillProgress(Alpha);

	// 머티리얼 파라미터 업데이트
	DynamicMaterialInstance->SetScalarParameterValue(TEXT("FillProgress"), FillValue);

	// 애니메이션 완료 체크
	if (Alpha >= 1.0f)
	{
		OnAnimationComplete();
	}
}

void APGSkillIndicator::OnAnimationComplete()
{
	// 애니메이션 타이머 정리
	GetWorldTimerManager().ClearTimer(FillTimerHandle);
	bIsAnimating = false;

	// 자동 제거 설정이 되어있다면
	if (bAutoDestroy)
	{
		if (DestroyDelay > 0.0f)
		{
			// 지연 후 제거
			TWeakObjectPtr<APGSkillIndicator> WeakThis =  MakeWeakObjectPtr(this);
			GetWorldTimerManager().SetTimer(
				DestroyTimerHandle,
				[WeakThis]() 
				{
					if (WeakThis.IsValid())
					{
						WeakThis->Destroy();
					}
				},
				DestroyDelay,
				false // 한 번만 실행
			);
		}
		else
		{
			// 즉시 제거
			Destroy();
		}
	}
}

float APGSkillIndicator::CalculateFillProgress(float Alpha) const
{
	if (true == bUseSmoothAnimation)
	{
		// 부드러운 커브
		float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);
		return FMath::Lerp(-1.0f, 1.0f, SmoothAlpha);
	}
	
	// -1.0에서 1.0으로 선형 보간
	return FMath::Lerp(-1.0f, 1.0f, Alpha);
}

