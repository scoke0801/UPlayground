#include "PGUIPopup.h"
#include "Engine/World.h"
#include "TimerManager.h"

UPGUIPopup::UPGUIPopup(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    UILayer = EPGUILayer::Popup;
    LayerPriority = 0;
    bCanBeOccluded = false;
}

void UPGUIPopup::NativeConstruct()
{
    Super::NativeConstruct();
    
    // 포커스 설정으로 키보드 입력 받기
    if (bIsModal)
    {
        SetKeyboardFocus();
    }
}

void UPGUIPopup::NativeDestruct()
{
    if (GetWorld() && AutoCloseTimer.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(AutoCloseTimer);
    }
    
    Super::NativeDestruct();
}

void UPGUIPopup::ShowUI()
{
    Super::ShowUI();
    bIsClosing = false;
    
    // 자동 닫기 타이머 설정
    if (AutoCloseTime > 0.0f && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            AutoCloseTimer,
            this,
            &UPGUIPopup::AutoClose,
            AutoCloseTime,
            false
        );
    }
}

void UPGUIPopup::HideUI()
{
    if (!bIsClosing)
    {
        bIsClosing = true;
        
        // 타이머 정리
        if (GetWorld() && AutoCloseTimer.IsValid())
        {
            GetWorld()->GetTimerManager().ClearTimer(AutoCloseTimer);
        }
        
    }
    
    Super::HideUI();
}

void UPGUIPopup::ShowPopup(const FText& Title, const FText& Message, EPGUIPopupType Type)
{
    PopupType = Type;
    ShowUI();
}

void UPGUIPopup::ShowConfirmationPopup(const FText& Title, const FText& Message)
{
    PopupType = EPGUIPopupType::Confirmation;
    ShowUI();
}

void UPGUIPopup::SetAutoCloseTime(float Time)
{
    AutoCloseTime = Time;
    
    // 이미 표시 중이면 타이머 재설정
    if (GetVisibility() == ESlateVisibility::Visible && GetWorld())
    {
        if (AutoCloseTimer.IsValid())
        {
            GetWorld()->GetTimerManager().ClearTimer(AutoCloseTimer);
        }
        
        if (AutoCloseTime > 0.0f)
        {
            GetWorld()->GetTimerManager().SetTimer(
                AutoCloseTimer,
                this,
                &UPGUIPopup::AutoClose,
                AutoCloseTime,
                false
            );
        }
    }
}

void UPGUIPopup::ClosePopup(bool bConfirmed)
{
    OnPopupClosed.Broadcast(bConfirmed);
    HideUI();
}

FReply UPGUIPopup::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (bCanCloseWithEscape && InKeyEvent.GetKey() == EKeys::Escape)
    {
        ClosePopup(false);
        return FReply::Handled();
    }
    
    // Enter 키로 확인
    if (InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::SpaceBar)
    {
        if (PopupType == EPGUIPopupType::Confirmation)
        {
            OnConfirmClicked();
        }
        else
        {
            ClosePopup(true);
        }
        return FReply::Handled();
    }
    
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPGUIPopup::AutoClose()
{
    ClosePopup(false);
}

void UPGUIPopup::OnConfirmClicked()
{
    ClosePopup(true);
}

void UPGUIPopup::OnCancelClicked()
{
    ClosePopup(false);
}
