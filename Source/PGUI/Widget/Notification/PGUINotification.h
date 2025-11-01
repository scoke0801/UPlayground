#pragma once

#include "CoreMinimal.h"
#include "../Base/PGWidgetBase.h"
#include "Engine/TimerHandle.h"
#include "PGUINotification.generated.h"

UENUM(BlueprintType)
enum class EPGUINotificationType : uint8
{
    Info,
    Success,
    Warning,
    Error,
    ItemAcquired,
    Achievement,
    SystemMessage,
    LevelUp,
    QuestComplete
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNotificationClosed, class UPGUINotification*, Notification);

/**
 * 시스템 메시지, 아이템 획득 등을 표시하는 알림 UI
 * 자동으로 사라지며 여러 개가 동시에 표시될 수 있음
 */
UCLASS(BlueprintType, Blueprintable)
class PGUI_API UPGUINotification : public UPGWidgetBase
{
    GENERATED_BODY()
    
protected:
    // 알림 타입
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Notification")
    EPGUINotificationType NotificationType = EPGUINotificationType::Info;

    // 표시 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Notification")
    float DisplayDuration = 3.0f;

    // 페이드 인 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Notification")
    float FadeInTime = 0.3f;

    // 페이드 아웃 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Notification")
    float FadeOutTime = 0.5f;

    // 클릭으로 닫을 수 있는지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PG|Notification")
    bool bCanCloseWithClick = true;

    // 알림 내용
    UPROPERTY(BlueprintReadOnly, Category = "PG|Notification")
    FText NotificationMessage;

    UPROPERTY(BlueprintReadOnly, Category = "PG|Notification")
    FText NotificationTitle;

private:
    FTimerHandle DisplayTimer;
    FTimerHandle FadeTimer;
    bool bIsClosing = false;
    bool bIsFadingOut = false;

public:
    UPGUINotification(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

public:
    // 알림 닫힘 이벤트
    UPROPERTY(BlueprintAssignable, Category = "PG|Notification")
    FOnNotificationClosed OnNotificationClosed;

    // 알림 설정 및 표시
    UFUNCTION(BlueprintCallable, Category = "PG|Notification")
    void ShowNotification(const FText& Message, EPGUINotificationType Type = EPGUINotificationType::Info, const FText& Title = FText::GetEmpty());
    
    // 표시 시간 설정
    UFUNCTION(BlueprintCallable, Category = "PG|Notification")
    void SetDisplayDuration(float Duration);

    // 수동으로 알림 닫기
    UFUNCTION(BlueprintCallable, Category = "PG|Notification")
    void CloseNotification();

protected:
    // 마우스 클릭 처리
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    UFUNCTION()
    void StartHideProcess();

    UFUNCTION()
    void FinishHiding();
};
