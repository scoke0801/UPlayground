#include "PGUIWindow.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"

UPGUIWindow::UPGUIWindow(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    UILayer = EPGUILayer::Window;
    LayerPriority = 0;
    bCanBeOccluded = false;
}

void UPGUIWindow::NativeConstruct()
{
    Super::NativeConstruct();
    
    // 포커스 설정으로 입력 이벤트 받기
    SetKeyboardFocus();
}

void UPGUIWindow::NativeDestruct()
{
    Super::NativeDestruct();
}

void UPGUIWindow::ShowUI()
{
    Super::ShowUI();
    bIsClosing = false;
}

void UPGUIWindow::HideUI()
{
    if (!bIsClosing)
    {
        bIsClosing = true;
    }
    
    Super::HideUI();
}

void UPGUIWindow::CloseWindow()
{
    HideUI();
}

void UPGUIWindow::ForceCloseWindow()
{
    bIsClosing = true;
    HideUI();
}

FReply UPGUIWindow::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (bCanCloseWithEscape && InKeyEvent.GetKey() == EKeys::Escape)
    {
        CloseWindow();
        return FReply::Handled();
    }
    
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UPGUIWindow::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bCanCloseWithBackgroundClick && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // 배경 클릭인지 확인 (자식 위젯이 클릭되지 않았는지)
        FVector2D LocalMousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        FVector2D LocalSize = InGeometry.GetLocalSize();
        
        if (LocalMousePos.X >= 0 && LocalMousePos.Y >= 0 && 
            LocalMousePos.X <= LocalSize.X && LocalMousePos.Y <= LocalSize.Y)
        {
            CloseWindow();
            return FReply::Handled();
        }
    }
    
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
