// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "NiagaraSystem.h"
#include "PGButton.generated.h"

class UNiagaraSystem;

/**
 * Niagara VFX를 지원하는 확장 버튼 위젯
 * - 버튼 클릭 시 UI 위치에서 월드 공간으로 변환하여 파티클 이펙트 재생
 * 
 * 사용법:
 * 1. Widget Blueprint 생성 시 부모를 PGButton으로 설정
 * 2. Details에서 Click Effect System 할당
 * 3. (선택) Effect Spawn Distance 조정 (기본 200 유닛)
 * 4. 버튼 클릭 시 자동으로 버튼 위치에 파티클 재생
 */
UCLASS()
class PGUI_API UPGButton : public UButton
{
	GENERATED_BODY()

protected:
	// === Niagara VFX 설정 ===
	
	/** 클릭 시 재생할 Niagara System */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Button|VFX")
	TObjectPtr<UNiagaraSystem> ClickEffectSystem;
	
	/** VFX 스폰 거리 (카메라로부터) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Button|VFX", meta = (ClampMin = "10.0", ClampMax = "1000.0"))
	float EffectSpawnDistance = 200.0f;
	
	/** VFX 스케일 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PG|Button|VFX", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float EffectScale = 1.0f;

private:
	/** 클릭 이벤트가 바인딩되었는지 체크 */
	bool bIsEventBound = false;

public:
	// UWidget 인터페이스
	virtual void SynchronizeProperties() override;

	/** 클릭 이펙트 재생 */
	UFUNCTION(BlueprintCallable, Category = "PG|Button|VFX")
	void PlayClickEffect();
	
protected:
	/** 버튼 클릭 이벤트 핸들러 */
	UFUNCTION()
	void OnButtonClicked();
};
