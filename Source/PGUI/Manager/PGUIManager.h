#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "../Widget/Base/PGWidgetBase.h"
#include "../Widget/Window/PGUIWindow.h"
#include "../Widget/HUD/PGUIHUD.h"
#include "../Widget/Popup/PGUIPopup.h"
#include "../Widget/Notification/PGUINotification.h"
#include "PGUIManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWindowStateChanged, bool, bWindowOpen);

/**
 * UI 전체를 관리하는 매니저 클래스
 * 게임 인스턴스 서브시스템으로 구현되어 전역적으로 접근 가능
 */
UCLASS(BlueprintType)
class PGUI_API UPGUIManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

private:
    // 현재 활성화된 UI들
    UPROPERTY()
    TArray<UPGWidgetBase*> ActiveWidgets;

    // 현재 열린 윈도우 (하나만 열릴 수 있음)
    UPROPERTY()
    UPGUIWindow* CurrentWindow = nullptr;

    // 메인 HUD
    UPROPERTY()
    UPGUIHUD* MainHUD = nullptr;

    // 활성화된 팝업들
    UPROPERTY()
    TArray<UPGUIPopup*> ActivePopups;

    // 활성화된 알림들
    UPROPERTY()
    TArray<UPGUINotification*> ActiveNotifications;

    // 알림 표시 위치 오프셋
    float NotificationVerticalOffset = 0.0f;
    const float NotificationSpacing = 80.0f; // 알림 간 간격

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

public:
    // 윈도우 상태 변경 이벤트
    UPROPERTY(BlueprintAssignable, Category = "PG|UI Manager")
    FOnWindowStateChanged OnWindowStateChanged;

    // UI 생성 및 표시
    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    UPGWidgetBase* CreateAndShowUI(TSubclassOf<UPGWidgetBase> WidgetClass);

    // 윈도우 관리
    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    bool OpenWindow(TSubclassOf<UPGUIWindow> WindowClass);

    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    void CloseCurrentWindow();

    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    bool IsWindowOpen() const { return CurrentWindow != nullptr; }

    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    UPGUIWindow* GetCurrentWindow() const { return CurrentWindow; }

    // HUD 관리
    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    void SetMainHUD(TSubclassOf<UPGUIHUD> HUDClass);

    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    UPGUIHUD* GetMainHUD() const { return MainHUD; }

    // 팝업 관리
    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    UPGUIPopup* ShowPopup(TSubclassOf<UPGUIPopup> PopupClass, const FText& Title, const FText& Message, EPGUIPopupType Type = EPGUIPopupType::Information);

    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    UPGUIPopup* ShowConfirmationPopup(TSubclassOf<UPGUIPopup> PopupClass, const FText& Title, const FText& Message);

    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    void CloseAllPopups();

    // 알림 관리
    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    UPGUINotification* ShowNotification(TSubclassOf<UPGUINotification> NotificationClass, const FText& Message, EPGUINotificationType Type = EPGUINotificationType::Info, const FText& Title = FText::GetEmpty());

    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    UPGUINotification* ShowItemAcquiredNotification(TSubclassOf<UPGUINotification> NotificationClass, const FText& ItemName, int32 Quantity = 1, const FText& ItemIcon = FText::GetEmpty());

    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    UPGUINotification* ShowAchievementNotification(TSubclassOf<UPGUINotification> NotificationClass, const FText& AchievementName, const FText& Description = FText::GetEmpty());

    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    UPGUINotification* ShowLevelUpNotification(TSubclassOf<UPGUINotification> NotificationClass, int32 NewLevel, const FText& CharacterName = FText::GetEmpty());

    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    void ClearAllNotifications();

    // UI 정리
    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    void CleanupInactiveWidgets();

    UFUNCTION(BlueprintCallable, Category = "PG|UI Manager")
    void CloseAllUI();

protected:
    void UpdateWidgetZOrders();
    void RepositionNotifications();

    // 이벤트 핸들러
    UFUNCTION()
    void OnNotificationClosed(UPGUINotification* Notification);

    UFUNCTION()
    void OnPopupClosed(bool bConfirmed);

private:
    void RegisterWidget(UPGWidgetBase* Widget);
    void UnregisterWidget(UPGWidgetBase* Widget);
    APlayerController* GetFirstPlayerController() const;
};
