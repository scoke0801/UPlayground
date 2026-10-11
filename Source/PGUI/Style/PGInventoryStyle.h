#pragma once

#include "PGUIStyle.h"

/** Cel-shaded preparation screens share typography, but not the global HUD palette. */
inline const FPGUIStyle& PGInventoryStyle()
{
    static const FPGUIStyle Style = []
    {
        FPGUIStyle Result = FPGUIStyle::Get();
        Result.Text = FLinearColor(.92f,.87f,.75f);
        Result.Muted = FLinearColor(.51f,.57f,.63f);
        Result.Mint = FLinearColor(.38f,.80f,.72f);
        Result.Lavender = FLinearColor(.85f,.65f,.32f);
        const FLinearColor Edge(.27f,.22f,.14f);
        Result.Panel = FSlateRoundedBoxBrush(FLinearColor(.012f,.018f,.027f,1.f),1.f,Edge,1.f);
        Result.Card = FSlateRoundedBoxBrush(FLinearColor(.024f,.033f,.046f,1.f),1.f,Edge,1.f);
        Result.Hover = FSlateRoundedBoxBrush(FLinearColor(.075f,.088f,.10f),1.f,Result.Text,1.f);
        Result.Pressed = FSlateRoundedBoxBrush(FLinearColor(.02f,.025f,.03f),1.f,Result.Lavender,2.f);
        Result.Selected = FSlateRoundedBoxBrush(FLinearColor(.065f,.056f,.036f),1.f,Result.Lavender,2.f);
        Result.Button.SetNormal(Result.Card).SetHovered(Result.Hover).SetPressed(Result.Pressed).SetDisabled(Result.Card);
        return Result;
    }();
    return Style;
}
