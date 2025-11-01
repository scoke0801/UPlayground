#include "PGUIHUD.h"
#include "Engine/World.h"

UPGUIHUD::UPGUIHUD(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    UILayer = EPGUILayer::HUD;
    LayerPriority = 0;
    bCanBeOccluded = true;
}

void UPGUIHUD::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshHUD();
}

void UPGUIHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    if (UpdateInterval > 0.0f)
    {
        LastUpdateTime += InDeltaTime;
        if (LastUpdateTime >= UpdateInterval)
        {
            RefreshHUD();
            LastUpdateTime = 0.0f;
        }
    }
    else
    {
        // 매 프레임 업데이트
        RefreshHUD();
    }
}

void UPGUIHUD::RefreshHUD()
{
    if (GetVisibility() == ESlateVisibility::Visible)
    {
        UpdatePlayerInfo();
        UpdateMiniMap();
        UpdateGameStatus();
    }
}

void UPGUIHUD::OnWindowStateChanged(bool bWindowOpen)
{
    if (bHideWhenWindowOpen)
    {
        if (bWindowOpen)
        {
            HideUI();
        }
        else
        {
            ShowUI();
        }
    }
}

void UPGUIHUD::SetHUDElementVisibility(const FString& ElementName, bool bVisible)
{
    // Blueprint에서 구현할 함수 - 특정 HUD 요소의 표시/숨김
    // ElementName에 따라 해당 위젯을 찾아서 Visibility 설정
}
