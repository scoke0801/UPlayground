#include "PGWidgetBase.h"
#include "Components/CanvasPanelSlot.h"

UPGWidgetBase::UPGWidgetBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(true);
}

void UPGWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Z-Order는 AddToViewport에서 설정
}

void UPGWidgetBase::NativeDestruct()
{
    Super::NativeDestruct();
}

void UPGWidgetBase::ShowUI()
{
    SetVisibility(ESlateVisibility::Visible);
    OnUIShown();
}

void UPGWidgetBase::HideUI()
{
    SetVisibility(ESlateVisibility::Hidden);
    OnUIHidden();
}

int32 UPGWidgetBase::GetCalculatedZOrder() const
{
    return static_cast<int32>(UILayer) + LayerPriority;
}
