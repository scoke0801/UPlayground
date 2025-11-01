#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGWidgetBase.generated.h"

UENUM(BlueprintType)
enum class EPGUILayer : uint8
{
    HUD = 0,        // Z-Order: 0-19
    Popup = 20,     // Z-Order: 20-39
    Window = 40,    // Z-Order: 40-59
    Notification = 60  // Z-Order: 60-79 (시스템 메시지용)
};

/**
 * 게임에서 사용할 UI의 기본 클래스
 * 모든 UI는 이 클래스를 상속받아 레이어 기반 관리 시스템을 사용한다
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PGUI_API UPGWidgetBase : public UUserWidget
{
    GENERATED_BODY()

public:
    UPGWidgetBase(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // UI 레이어 타입
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PGUI")
    EPGUILayer UILayer = EPGUILayer::HUD;

    // 레이어 내에서의 우선순위 (높을수록 앞에 표시)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PGUI")
    int32 LayerPriority = 0;

    // 다른 UI에 의해 가려질 수 있는지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PGUI")
    bool bCanBeOccluded = true;

public:
    // UI 표시/숨김
    UFUNCTION(BlueprintCallable, Category = "PGUI")
    virtual void ShowUI();

    UFUNCTION(BlueprintCallable, Category = "PGUI")
    virtual void HideUI();

    // Z-Order 계산
    UFUNCTION(BlueprintPure, Category = "PGUI")
    int32 GetCalculatedZOrder() const;

    // 레이어 정보 접근자
    UFUNCTION(BlueprintPure, Category = "PGUI")
    EPGUILayer GetUILayer() const { return UILayer; }

    UFUNCTION(BlueprintPure, Category = "PGUI")
    int32 GetLayerPriority() const { return LayerPriority; }

protected:
    // UI 상태 변경 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "PGUI")
    void OnUIShown();

    UFUNCTION(BlueprintImplementableEvent, Category = "PGUI")
    void OnUIHidden();
};
