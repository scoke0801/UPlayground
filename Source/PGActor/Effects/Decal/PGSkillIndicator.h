// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGDecalActor.h"
#include "PGSkillIndicator.generated.h"

// 스킬 인디케이터 완료 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillIndicatorCompleted, APGSkillIndicator*, SkillIndicator);

/**
 * 
 */
UCLASS()
class PGACTOR_API APGSkillIndicator : public APGDecalActor
{
	GENERATED_BODY()

protected:
	
	// 베이스 머티리얼 (에디터에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Material")
	UMaterialInterface* DecalMaterial;
	
	// 애니메이션 완료 후 자동 제거 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Animation")
	bool bAutoDestroy = true;

	// 애니메이션 완료 후 대기 시간 (자동 제거 시)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Animation", meta = (EditCondition = "bAutoDestroy"))
	float DestroyDelay = 1.0f;

	// 자동 시작 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Animation")
	bool bAutoStartAnimation = true;

	// 애니메이션 시간
	UPROPERTY(EditDefaultsOnly, Category = "PG|Animation")
	float AnimationDuration = 1.0f;

	// 부드러운 보간 사용여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Animation")
	bool bUseSmoothAnimation = false;
	
private:
	// 타이머 핸들
	FTimerHandle FillTimerHandle;
	FTimerHandle DestroyTimerHandle;
    
	// 애니메이션 진행 상태
	float CurrentTime = 0.0f;
	bool bIsAnimating = false;

	// 다이나믹 머티리얼 인스턴스
	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterialInstance;
	
public:
	APGSkillIndicator();

	// 애니메이션 완료 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "PG|Animation")
	FOnSkillIndicatorCompleted OnAnimationCompleted;
	
protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	
public:
	UFUNCTION(BlueprintCallable, Category = "PG|Animation")
	void StartAnimation();
	UFUNCTION(BlueprintCallable, Category = "PG|Animation")
	void EndAnimation();

	UFUNCTION(BlueprintCallable, Category = "PG|Animation")
	bool IsAnimating() const { return bIsAnimating; }

	UFUNCTION(BlueprintCallable, Category = "PG|Animation")
	float GetAnimationProgress() const;

private:
	// 애니메이션 함수들
	UFUNCTION()
	void UpdateFillProgress();
    
	UFUNCTION()
	void OnAnimationComplete();
    
	// FillProgress 값 계산 (-1.0 → 1.0)
	float CalculateFillProgress(float Alpha) const;
};
