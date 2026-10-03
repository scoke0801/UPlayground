#include "PGUIStyle.h"
#include "PGUIStyleSettings.h"
#include "Styling/CoreStyle.h"

const FPGUIStyle& FPGUIStyle::Get()
{
    static const FPGUIStyle Style;
    return Style;
}

FPGUIStyle::FPGUIStyle()
    : Mint(GetDefault<UPGUIStyleSettings>()->Accent), Lavender(GetDefault<UPGUIStyleSettings>()->Secondary),
      Text(GetDefault<UPGUIStyleSettings>()->Text), Muted(GetDefault<UPGUIStyleSettings>()->Muted),
      Danger(GetDefault<UPGUIStyleSettings>()->Danger), Rare(GetDefault<UPGUIStyleSettings>()->Rare), Magic(GetDefault<UPGUIStyleSettings>()->Magic),
      Panel(GetDefault<UPGUIStyleSettings>()->Panel, 12.f, FLinearColor(.10f,.16f,.23f,.7f), 1.f),
      Card(GetDefault<UPGUIStyleSettings>()->Card, 8.f, FLinearColor(.10f,.16f,.23f,.7f), 1.f),
      Hover(FLinearColor(.065f,.13f,.16f), 8.f, Mint, 1.5f),
      Pressed(FLinearColor(.025f,.075f,.09f), 8.f, Mint, 2.f),
      Selected(FLinearColor(.035f,.10f,.12f), 8.f, Mint, 2.f),
      Cooldown(FLinearColor(.008f,.012f,.03f,.78f), 8.f)
{
    Button.SetNormal(Card).SetHovered(Hover).SetPressed(Pressed).SetDisabled(Card)
        .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
}

FSlateFontInfo FPGUIStyle::Font(int32 Size, bool bBold) const
{
    return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size > 0 ? Size : GetDefault<UPGUIStyleSettings>()->BodyFontSize);
}

FLinearColor FPGUIStyle::RarityColor(EPGItemRarity Rarity) const
{
    return Rarity == EPGItemRarity::Rare ? Rare : Rarity == EPGItemRarity::Magic ? Magic : Muted;
}
