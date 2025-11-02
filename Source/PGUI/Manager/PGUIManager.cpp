#include "PGUIManager.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/AssetPath/PGUIWidgetPathRow.h"

TWeakObjectPtr<UPGUIManager> UPGUIManager::WeakThis = nullptr;

UPGUIManager* UPGUIManager::Get()
{
    if (WeakThis.IsValid())
    {
        return WeakThis.Get();
    }
    return nullptr;
}

void UPGUIManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    WeakThis = MakeWeakObjectPtr(this);
    
    UE_LOG(LogTemp, Log, TEXT("PGUIManager Initialized"));
}

void UPGUIManager::Deinitialize()
{
    CloseAllUI();
    Super::Deinitialize();
}

UPGWidgetBase* UPGUIManager::CreateAndShowUI(TSubclassOf<UPGWidgetBase> WidgetClass)
{
    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("PGUIManager::CreateAndShowUI - WidgetClass is null"));
        return nullptr;
    }

    APlayerController* PC = GetFirstPlayerController();
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("PGUIManager::CreateAndShowUI - No valid PlayerController found"));
        return nullptr;
    }

    UPGWidgetBase* NewWidget = CreateWidget<UPGWidgetBase>(PC, WidgetClass);
    if (NewWidget)
    {
        RegisterWidget(NewWidget);
        NewWidget->AddToViewport(NewWidget->GetCalculatedZOrder());
        NewWidget->ShowUI();
    }

    return NewWidget;
}

bool UPGUIManager::OpenWindow(TSubclassOf<UPGUIWindow> WindowClass)
{
    if (!WindowClass)
    {
        return false;
    }

    // 이미 윈도우가 열려있으면 닫기
    if (CurrentWindow)
    {
        CloseCurrentWindow();
    }

    APlayerController* PC = GetFirstPlayerController();
    if (!PC)
    {
        return false;
    }

    CurrentWindow = CreateWidget<UPGUIWindow>(PC, WindowClass);
    if (CurrentWindow)
    {
        RegisterWidget(CurrentWindow);
        CurrentWindow->AddToViewport(CurrentWindow->GetCalculatedZOrder());
        CurrentWindow->ShowUI();
        
        // HUD 숨기기
        if (MainHUD)
        {
            MainHUD->OnWindowStateChanged(true);
        }
        
        OnWindowStateChanged.Broadcast(true);
        return true;
    }

    return false;
}

UPGWidgetBase* UPGUIManager::OpenAndGetWidget(const EPGUIWIdgetEnumTypes& InKey)
{
    UPGDataTableManager* tableManager= PGData();
    if (nullptr == tableManager)
    {
        return nullptr;
    }
    
    FPGUIWidgetPathRow* Data = PGData()->GetRowData<FPGUIWidgetPathRow>(static_cast<int64>(InKey));
    if (nullptr == Data)
    {
        return nullptr;
    }
    TSoftClassPtr<UPGWidgetBase> SoftClassPtr(Data->ClassPath);

    if (UClass* LoadedClass = SoftClassPtr.LoadSynchronous())
    {
        // 현재 월드 가져오기
        if (UWorld* World = GetWorld())
        {
            return CreateWidget<UPGWidgetBase>(World, LoadedClass);
        }
    }
    
    return nullptr;
}

void UPGUIManager::CloseCurrentWindow()
{
    if (CurrentWindow)
    {
        CurrentWindow->HideUI();
        UnregisterWidget(CurrentWindow);
        CurrentWindow = nullptr;
        
        // HUD 다시 표시
        if (MainHUD)
        {
            MainHUD->OnWindowStateChanged(false);
        }
        
        OnWindowStateChanged.Broadcast(false);
    }
}

void UPGUIManager::SetMainHUD(TSubclassOf<UPGUIHUD> HUDClass)
{
    if (!HUDClass)
    {
        return;
    }

    // 기존 HUD 제거
    if (MainHUD)
    {
        MainHUD->HideUI();
        UnregisterWidget(MainHUD);
        MainHUD = nullptr;
    }

    APlayerController* PC = GetFirstPlayerController();
    if (!PC)
    {
        return;
    }

    MainHUD = CreateWidget<UPGUIHUD>(PC, HUDClass);
    if (MainHUD)
    {
        RegisterWidget(MainHUD);
        MainHUD->AddToViewport(MainHUD->GetCalculatedZOrder());
        MainHUD->ShowUI();
    }
}

UPGUIPopup* UPGUIManager::ShowPopup(TSubclassOf<UPGUIPopup> PopupClass, const FText& Title, const FText& Message, EPGUIPopupType Type)
{
    if (!PopupClass)
    {
        return nullptr;
    }

    APlayerController* PC = GetFirstPlayerController();
    if (!PC)
    {
        return nullptr;
    }

    UPGUIPopup* NewPopup = CreateWidget<UPGUIPopup>(PC, PopupClass);
    if (NewPopup)
    {
        RegisterWidget(NewPopup);
        ActivePopups.Add(NewPopup);
        
        // 이벤트 바인딩
        NewPopup->OnPopupClosed.AddDynamic(this, &UPGUIManager::OnPopupClosed);
        
        NewPopup->AddToViewport(NewPopup->GetCalculatedZOrder());
        NewPopup->ShowPopup(Title, Message, Type);
    }

    return NewPopup;
}

UPGUIPopup* UPGUIManager::ShowConfirmationPopup(TSubclassOf<UPGUIPopup> PopupClass, const FText& Title, const FText& Message)
{
    return ShowPopup(PopupClass, Title, Message, EPGUIPopupType::Confirmation);
}

void UPGUIManager::CloseAllPopups()
{
    for (UPGUIPopup* Popup : ActivePopups)
    {
        if (Popup)
        {
            Popup->ClosePopup(false);
        }
    }
    ActivePopups.Empty();
}

UPGUINotification* UPGUIManager::ShowNotification(TSubclassOf<UPGUINotification> NotificationClass, const FText& Message, EPGUINotificationType Type, const FText& Title)
{
    if (!NotificationClass)
    {
        return nullptr;
    }

    APlayerController* PC = GetFirstPlayerController();
    if (!PC)
    {
        return nullptr;
    }

    UPGUINotification* NewNotification = CreateWidget<UPGUINotification>(PC, NotificationClass);
    if (NewNotification)
    {
        RegisterWidget(NewNotification);
        ActiveNotifications.Add(NewNotification);
        
        // 이벤트 바인딩
        NewNotification->OnNotificationClosed.AddDynamic(this, &UPGUIManager::OnNotificationClosed);
        
        NewNotification->AddToViewport(NewNotification->GetCalculatedZOrder());
        NewNotification->ShowNotification(Message, Type, Title);
        RepositionNotifications();
    }

    return NewNotification;
}

UPGUINotification* UPGUIManager::ShowItemAcquiredNotification(TSubclassOf<UPGUINotification> NotificationClass, const FText& ItemName, int32 Quantity, const FText& ItemIcon)
{
    if (!NotificationClass)
    {
        return nullptr;
    }

    APlayerController* PC = GetFirstPlayerController();
    if (!PC)
    {
        return nullptr;
    }

    UPGUINotification* NewNotification = CreateWidget<UPGUINotification>(PC, NotificationClass);
    if (NewNotification)
    {
        RegisterWidget(NewNotification);
        ActiveNotifications.Add(NewNotification);
        
        // 이벤트 바인딩
        NewNotification->OnNotificationClosed.AddDynamic(this, &UPGUIManager::OnNotificationClosed);
        
        NewNotification->AddToViewport(NewNotification->GetCalculatedZOrder());
        RepositionNotifications();
    }

    return NewNotification;
}

UPGUINotification* UPGUIManager::ShowAchievementNotification(TSubclassOf<UPGUINotification> NotificationClass, const FText& AchievementName, const FText& Description)
{
    if (!NotificationClass)
    {
        return nullptr;
    }

    APlayerController* PC = GetFirstPlayerController();
    if (!PC)
    {
        return nullptr;
    }

    UPGUINotification* NewNotification = CreateWidget<UPGUINotification>(PC, NotificationClass);
    if (NewNotification)
    {
        RegisterWidget(NewNotification);
        ActiveNotifications.Add(NewNotification);
        
        // 이벤트 바인딩
        NewNotification->OnNotificationClosed.AddDynamic(this, &UPGUIManager::OnNotificationClosed);
        
        NewNotification->AddToViewport(NewNotification->GetCalculatedZOrder());
        RepositionNotifications();
    }

    return NewNotification;
}

UPGUINotification* UPGUIManager::ShowLevelUpNotification(TSubclassOf<UPGUINotification> NotificationClass, int32 NewLevel, const FText& CharacterName)
{
    if (!NotificationClass)
    {
        return nullptr;
    }

    APlayerController* PC = GetFirstPlayerController();
    if (!PC)
    {
        return nullptr;
    }

    UPGUINotification* NewNotification = CreateWidget<UPGUINotification>(PC, NotificationClass);
    if (NewNotification)
    {
        RegisterWidget(NewNotification);
        ActiveNotifications.Add(NewNotification);
        
        // 이벤트 바인딩
        NewNotification->OnNotificationClosed.AddDynamic(this, &UPGUIManager::OnNotificationClosed);
        
        NewNotification->AddToViewport(NewNotification->GetCalculatedZOrder());
        RepositionNotifications();
    }

    return NewNotification;
}

void UPGUIManager::ClearAllNotifications()
{
    for (UPGUINotification* Notification : ActiveNotifications)
    {
        if (Notification)
        {
            Notification->CloseNotification();
        }
    }
    ActiveNotifications.Empty();
    NotificationVerticalOffset = 0.0f;
}

void UPGUIManager::CleanupInactiveWidgets()
{
    ActiveWidgets.RemoveAll([](UPGWidgetBase* Widget) {
        return !IsValid(Widget) || Widget->GetVisibility() == ESlateVisibility::Hidden;
    });
    
    ActivePopups.RemoveAll([](UPGUIPopup* Popup) {
        return !IsValid(Popup) || Popup->GetVisibility() == ESlateVisibility::Hidden;
    });
    
    ActiveNotifications.RemoveAll([](UPGUINotification* Notification) {
        return !IsValid(Notification) || Notification->GetVisibility() == ESlateVisibility::Hidden;
    });
}

void UPGUIManager::CloseAllUI()
{
    CloseCurrentWindow();
    CloseAllPopups();
    ClearAllNotifications();
    
    if (MainHUD)
    {
        MainHUD->HideUI();
        UnregisterWidget(MainHUD);
        MainHUD = nullptr;
    }
    
    ActiveWidgets.Empty();
}

void UPGUIManager::UpdateWidgetZOrders()
{
    // Z-Order는 AddToViewport에서 설정되므로 별도 처리 불필요
    // 필요시 위젯들을 다시 AddToViewport 호출
}

void UPGUIManager::RepositionNotifications()
{
    float CurrentOffset = 100.0f; // 화면 상단에서의 시작 오프셋
    
    for (int32 i = 0; i < ActiveNotifications.Num(); ++i)
    {
        UPGUINotification* Notification = ActiveNotifications[i];
        if (IsValid(Notification))
        {
            // 알림 위치를 위에서부터 아래로 배치
            // Blueprint에서 위치 설정을 위한 이벤트 호출 필요
            // 여기서는 개념적인 구현만 제공
            CurrentOffset += NotificationSpacing;
        }
    }
    
    NotificationVerticalOffset = CurrentOffset;
}

void UPGUIManager::OnNotificationClosed(UPGUINotification* Notification)
{
    if (Notification)
    {
        ActiveNotifications.Remove(Notification);
        UnregisterWidget(Notification);
        RepositionNotifications();
    }
}

void UPGUIManager::OnPopupClosed(bool bConfirmed)
{
    // 팝업이 닫혔을 때의 처리
    CleanupInactiveWidgets();
}

void UPGUIManager::RegisterWidget(UPGWidgetBase* Widget)
{
    if (Widget && !ActiveWidgets.Contains(Widget))
    {
        ActiveWidgets.Add(Widget);
    }
}

void UPGUIManager::UnregisterWidget(UPGWidgetBase* Widget)
{
    if (Widget)
    {
        ActiveWidgets.Remove(Widget);
    }
}

APlayerController* UPGUIManager::GetFirstPlayerController() const
{
    UWorld* World = GetWorld();
    if (World)
    {
        return World->GetFirstPlayerController();
    }
    return nullptr;
}
