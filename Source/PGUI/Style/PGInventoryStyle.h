#pragma once

#include "PGUIStyle.h"
#include "PGUIStyleSettings.h"

/** Dark stone and aged metal for inventory and character sheets. */
inline const FPGUIStyle& PGInventoryStyle()
{
    static const FPGUIStyle Style = []
    {
        FPGUIStyle Result = FPGUIStyle::Get();
        const auto* Settings = GetDefault<UPGUIStyleSettings>();
        Result.Text = Settings->InventoryText;
        Result.Muted = Settings->InventoryMuted;
        Result.Mint = FLinearColor(.38f,.80f,.72f);
        Result.Lavender = Settings->InventoryAccent;
        Result.Rare = Settings->InventoryRare;
        Result.Magic = FLinearColor(.48f,.58f,.64f);
        const FLinearColor Edge(.28f,.27f,.24f);
        Result.Panel = FSlateRoundedBoxBrush(FLinearColor(.012f,.018f,.027f,1.f),1.f,Edge,1.f);
        Result.Card = FSlateRoundedBoxBrush(FLinearColor(.006f,.007f,.008f,1.f),1.f,FLinearColor(.25f,.23f,.19f),1.f);
        Result.Hover = FSlateRoundedBoxBrush(FLinearColor(.065f,.055f,.039f),1.f,Result.Lavender,1.f);
        Result.Pressed = FSlateRoundedBoxBrush(FLinearColor(.02f,.025f,.03f),1.f,Result.Lavender,2.f);
        Result.Selected = FSlateRoundedBoxBrush(FLinearColor(.065f,.056f,.036f),1.f,Result.Lavender,2.f);
        Result.Button.SetNormal(Result.Card).SetHovered(Result.Hover).SetPressed(Result.Pressed).SetDisabled(Result.Card)
            .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0,1,0,-1));
        return Result;
    }();
    return Style;
}

inline const FButtonStyle& PGInventoryPrimaryStyle()
{
    static const FButtonStyle Style = []
    {
        FButtonStyle Result = PGInventoryStyle().Button;
        const auto Gold = PGInventoryStyle().Lavender;
        Result.SetNormal(FSlateRoundedBoxBrush(Gold,0.f));
        Result.SetHovered(FSlateRoundedBoxBrush(Gold*1.18f,1.f));
        Result.SetPressed(FSlateRoundedBoxBrush(Gold*.8f,1.f));
        return Result;
    }();
    return Style;
}

inline const FButtonStyle& PGInventorySlotStyle()
{
    static const FButtonStyle Style = []
    {
        FButtonStyle Result = PGInventoryStyle().Button;
        const FSlateRoundedBoxBrush Slot(FLinearColor(.008f,.009f,.010f,1.f),0.f,FLinearColor(.24f,.22f,.18f),1.f);
        Result.SetNormal(Slot).SetDisabled(Slot);
        return Result;
    }();
    return Style;
}

inline const FButtonStyle& PGInventoryTabStyle()
{
    static const FButtonStyle Style = []
    {
        FButtonStyle Result = PGInventoryStyle().Button;
        Result.SetNormal(FSlateRoundedBoxBrush(FLinearColor::White,0.f));
        return Result;
    }();
    return Style;
}

inline const FScrollBarStyle& PGInventoryScrollStyle()
{
    static const FScrollBarStyle Style = []
    {
        FScrollBarStyle Result;
        Result.SetNormalThumbImage(FSlateRoundedBoxBrush(PGInventoryStyle().Muted,1.f));
        Result.SetHoveredThumbImage(FSlateRoundedBoxBrush(PGInventoryStyle().Text,1.f));
        Result.SetDraggedThumbImage(FSlateRoundedBoxBrush(PGInventoryStyle().Lavender,1.f));
        return Result;
    }();
    return Style;
}
