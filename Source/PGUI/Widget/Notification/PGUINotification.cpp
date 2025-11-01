#include "PGUINotification.h"
#include "Engine/World.h"
#include "TimerManager.h"

UPGUINotification::UPGUINotification(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    UILayer = EPGUILayer::Notification;
    LayerPriority = 0;
    bCanBeOccluded = false;
}

void UPGUINotification::NativeConstruct()
{
    Super::NativeConstruct();
}

void UPGUINotification::NativeDestruct()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(DisplayTimer);
        GetWorld()->GetTimerManager().ClearTimer(FadeTimer);
    }
    
    Super::NativeDestruct();
}

void UPGUINotification::ShowNotification(const FText& Message, EPGUINotificationType Type, const FText& Title)
{
    NotificationType = Type;
    NotificationMessage = Message;
    NotificationTitle = Title;
    
    // 표시 타이머 설정
    if (DisplayDuration > 0.0f && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            DisplayTimer,
            this,
            &UPGUINotification::StartHideProcess,
            DisplayDuration,
            false
        );
    }
}

void UPGUINotification::SetDisplayDuration(float Duration)
{
    DisplayDuration = Duration;
    
    // 이미 표시 중이면 타이머 재설정
    if (GetVisibility() == ESlateVisibility::Visible && GetWorld())
    {
        if (DisplayTimer.IsValid())
        {
            GetWorld()->GetTimerManager().ClearTimer(DisplayTimer);
        }
        
        if (DisplayDuration > 0.0f)
        {
            GetWorld()->GetTimerManager().SetTimer(
                DisplayTimer,
                this,
                &UPGUINotification::StartHideProcess,
                DisplayDuration,
                false
            );
        }
    }
}

void UPGUINotification::CloseNotification()
{
    if (!bIsClosing)
    {
        StartHideProcess();
    }
}

FReply UPGUINotification::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bCanCloseWithClick && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        CloseNotification();
        return FReply::Handled();
    }
    
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UPGUINotification::StartHideProcess()
{
    if (bIsClosing) return;
    
    bIsClosing = true;
    bIsFadingOut = true;
    
    // 타이머 정리
    if (GetWorld() && DisplayTimer.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(DisplayTimer);
    }
    
    // 페이드 아웃 완료 후 제거
    if (FadeOutTime > 0.0f && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            FadeTimer,
            this,
            &UPGUINotification::FinishHiding,
            FadeOutTime,
            false
        );
    }
    else
    {
        FinishHiding(); 
    }
}

void UPGUINotification::FinishHiding()
{
    OnNotificationClosed.Broadcast(this);
    HideUI();
}
