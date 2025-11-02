#pragma once

#include "CoreMinimal.h"
#include "../Base/PGWidgetBase.h"
#include "Engine/TimerHandle.h"
#include "PGUIPopup.generated.h"

UENUM(BlueprintType)
enum class EPGUIPopupType : uint8
{
    Confirmation,   // 확인/취소 팝업
    Information,    // 정보 표시 팝업
    Warning,        // 경고 팝업
    Error,          // 에러 팝업
    Custom          // 커스텀 팝업
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPopupClosed, bool, bConfirmed);

/**
 * 팝업 UI 클래스
 * 사용자의 확인이나 정보 표시를 위한 팝업 윈도우
 */
UCLASS(BlueprintType, Blueprintable)
class PGUI_API UPGUIPopup : public UPGWidgetBase
{
    GENERATED_BODY()

protected:
    // 팝업 타입
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Popup")
    EPGUIPopupType PopupType = EPGUIPopupType::Information;

    // 모달 팝업인지 (다른 UI 입력 차단)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Popup")
    bool bIsModal = true;

    // 자동으로 닫히는 시간 (0이면 수동으로만 닫힘)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Popup")
    float AutoCloseTime = 0.0f;

    // ESC 키로 닫을 수 있는지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Popup")
    bool bCanCloseWithEscape = true;
    
private:
    FTimerHandle AutoCloseTimer;
    bool bIsClosing = false;

public:
    UPGUIPopup(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

public:
    // 팝업 닫힘 이벤트
    UPROPERTY(BlueprintAssignable, Category = "PG|Popup")
    FOnPopupClosed OnPopupClosed;

    virtual void ShowUI() override;
    virtual void HideUI() override;

    // 팝업 설정 및 표시
    UFUNCTION(BlueprintCallable, Category = "PG|Popup")
    void ShowPopup(const FText& Title, const FText& Message, EPGUIPopupType Type = EPGUIPopupType::Information);

    // 확인/취소 팝업 표시
    UFUNCTION(BlueprintCallable, Category = "PG|Popup")
    void ShowConfirmationPopup(const FText& Title, const FText& Message);

    // 자동 닫기 시간 설정
    UFUNCTION(BlueprintCallable, Category = "PG|Popup")
    void SetAutoCloseTime(float Time);

    // 팝업 닫기
    UFUNCTION(BlueprintCallable, Category = "PG|Popup")
    void ClosePopup(bool bConfirmed = false);

protected:
    // 입력 처리
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION()
    void AutoClose();

    // 버튼 클릭 이벤트
    UFUNCTION(BlueprintCallable, Category = "PG|Popup")
    void OnConfirmClicked();

    UFUNCTION(BlueprintCallable, Category = "PG|Popup")
    void OnCancelClicked();
    
};
