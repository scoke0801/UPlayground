#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"

/** Combat-only materials; inventory/reward themes retain their own presentation. */
struct FPGCombatHUDStyle
{
    const FLinearColor Ivory = FLinearColor(.88f,.81f,.66f);
    const FLinearColor Muted = FLinearColor(.48f,.43f,.34f);
    const FLinearColor Gold = FLinearColor(.63f,.40f,.16f);
    FSlateRoundedBoxBrush Panel{FLinearColor(.012f,.015f,.023f,.96f), 1.f, FLinearColor(.19f,.14f,.075f), 1.f};
    FSlateRoundedBoxBrush Slot{FLinearColor(.018f,.023f,.035f), 1.f, FLinearColor(.30f,.23f,.13f), 1.f};
    FSlateRoundedBoxBrush Hover{FLinearColor(.09f,.063f,.03f), 1.f, FLinearColor(.70f,.48f,.22f), 1.f};
    FSlateRoundedBoxBrush Pressed{FLinearColor(.012f,.008f,.005f), 1.f, FLinearColor(.45f,.25f,.08f), 1.f};
    FSlateRoundedBoxBrush Shade{FLinearColor(0,0,0,.72f), 0.f};
    FButtonStyle Button;
    FPGCombatHUDStyle()
    {
        Button.SetNormal(Slot).SetHovered(Hover).SetPressed(Pressed).SetDisabled(Slot);
    }
    static const FPGCombatHUDStyle& Get() { static FPGCombatHUDStyle Style; return Style; }
};
