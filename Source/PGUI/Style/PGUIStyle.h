#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "PGShared/Shared/Structure/PGInventoryTypes.h"

/** Process-lifetime brushes: Slate retains pointers to these resources. */
struct PGUI_API FPGUIStyle
{
    static const FPGUIStyle& Get();
    FLinearColor Mint, Lavender, Text, Muted, Danger, Rare, Magic;
    FSlateRoundedBoxBrush Panel, Card, Hover, Pressed, Selected, Cooldown;
    FButtonStyle Button;
    FSlateFontInfo Font(int32 Size = 0, bool bBold = false) const;
    FLinearColor RarityColor(EPGItemRarity Rarity) const;
private:
    FPGUIStyle();
};
