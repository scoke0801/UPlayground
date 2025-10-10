// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "NiagaraSystem.h"
#include "PGButton.generated.h"

class UNiagaraSystem;
class UWidgetComponent;
class UCanvasPanel;
class UImage;

/**
 * Niagara VFX를 지원하는 확장 버튼 위젯
 * - 버튼 클릭 시 스크린 좌표계에서 파티클 이펙트 재생
 * 
 * 사용법:
 * 1. Widget Blueprint 생성 시 부모를 PGButton으로 설정
 * 2. Details에서 Click Effect System 할당
 * 3. 버튼 클릭 시 자동으로 파티클 재생
 */
UCLASS()
class PGUI_API UPGButton : public UButton
{
	GENERATED_BODY()

protected:
	// === Niagara VFX 설정 ===
	
	// 클릭 시 재생할 Niagara System
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button|VFX")
	TObjectPtr<UNiagaraSystem> ClickEffectSystem;
	
	// 파티클 렌더링 방식
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button|VFX")
	bool bUseScreenSpace = true;  // true: 스크린 좌표계, false: 월드 좌표계
	
	// [월드 좌표계 전용] 파티클 효과가 버튼보다 앞에 렌더링될 오프셋 (Z축)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button|VFX", meta = (EditCondition = "!bUseScreenSpace"))
	float ParticleZOffset = 100.0f;
	
	// 파티클 효과의 스케일
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button|VFX")
	FVector ParticleScale = FVector(1.0f);
	
	// 버튼 중심에서의 오프셋 (픽셀 단위)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button|VFX")
	FVector2D ParticleOffset = FVector2D::ZeroVector;
	
	// 디버그 모드: 스폰 위치를 화면에 표시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button|VFX|Debug")
	bool bDebugShowSpawnLocation = false;

public:
	UPGButton(const FObjectInitializer& ObjectInitializer);

	// UWidget 인터페이스
	virtual void SynchronizeProperties() override;

protected:
	UFUNCTION()
	void OnButtonClicked();
	
	// VFX 재생 (Blueprint에서 오버라이드 가능)
	UFUNCTION(BlueprintNativeEvent, Category = "Button|VFX")
	void PlayClickEffect();
	virtual void PlayClickEffect_Implementation();
	
private:
	// === 스크린 스페이스 방식 ===
	void SpawnScreenSpaceEffect();
	UCanvasPanel* FindRootCanvasPanel() const;
	
	// === 월드 스페이스 방식 ===
	void SpawnWorldSpaceEffect();
	bool GetButtonWorldLocation(FVector& OutWorldLocation, FVector& OutWorldDirection) const;
	void SpawnNiagaraAtLocation(const FVector& WorldLocation, const FVector& WorldDirection);
	
	// 클릭 이벤트가 바인딩되었는지 체크
	bool bIsEventBound = false;
};
