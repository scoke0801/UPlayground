#pragma once

#include "CoreMinimal.h"
#include "../Base/PGWidgetBase.h"
#include "PGUIHUD.generated.h"

/**
 * 상시 표시되는 HUD UI
 * 게임 플레이 중 항상 보이는 UI 요소들을 관리
 */
UCLASS(BlueprintType, Blueprintable)
class PGUI_API UPGUIHUD : public UPGWidgetBase
{
    GENERATED_BODY()

protected:
    // 윈도우에 의해 숨겨질지 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|HUD")
    bool bHideWhenWindowOpen = true;

    // HUD 업데이트 주기 (초 단위, 0이면 매 프레임)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|HUD")
    float UpdateInterval = 0.1f;

private:
    float LastUpdateTime = 0.0f;
    
public:
    UPGUIHUD(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
    // HUD 업데이트 함수들
    UFUNCTION(BlueprintImplementableEvent, Category = "PG|HUD")
    void UpdatePlayerInfo();

    UFUNCTION(BlueprintImplementableEvent, Category = "PG|HUD")
    void UpdateMiniMap();

    UFUNCTION(BlueprintImplementableEvent, Category = "PG|HUD")
    void UpdateGameStatus();

    // 전체 HUD 업데이트
    UFUNCTION(BlueprintCallable, Category = "PG|HUD")
    void RefreshHUD();

    // 윈도우 상태 변경 시 호출
    UFUNCTION(BlueprintCallable, Category = "PG|HUD")
    void OnWindowStateChanged(bool bWindowOpen);

    // HUD 요소 개별 표시/숨김
    UFUNCTION(BlueprintCallable, Category = "PG|HUD")
    void SetHUDElementVisibility(const FString& ElementName, bool bVisible);
};
