#include "PGUIStyle.h"
#include "PGUIStyleSettings.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"
#include "Fonts/CompositeFont.h"

const FPGUIStyle& FPGUIStyle::Get()
{
    static const FPGUIStyle Style;
    return Style;
}

FPGUIStyle::FPGUIStyle()
    : Mint(GetDefault<UPGUIStyleSettings>()->Accent), Lavender(GetDefault<UPGUIStyleSettings>()->Secondary),
      Text(GetDefault<UPGUIStyleSettings>()->Text), Muted(GetDefault<UPGUIStyleSettings>()->Muted),
      Danger(GetDefault<UPGUIStyleSettings>()->Danger), Rare(GetDefault<UPGUIStyleSettings>()->Rare), Magic(GetDefault<UPGUIStyleSettings>()->Magic),
      Panel(GetDefault<UPGUIStyleSettings>()->Panel, 4.f, FLinearColor(.20f,.24f,.38f,.55f), 1.f),
      Card(GetDefault<UPGUIStyleSettings>()->Card, 4.f, FLinearColor(.20f,.24f,.38f,.55f), 1.f),
      Hover(FLinearColor(.065f,.08f,.15f), 4.f, Lavender, 1.5f),
      Pressed(FLinearColor(.035f,.045f,.10f), 4.f, Mint, 2.f),
      Selected(FLinearColor(.055f,.07f,.14f), 4.f, Lavender, 2.f),
      Cooldown(FLinearColor(.008f,.012f,.03f,.78f), 8.f)
{
    Button.SetNormal(Card).SetHovered(Hover).SetPressed(Pressed).SetDisabled(Card)
        .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
}

FSlateFontInfo FPGUIStyle::Font(int32 Size, bool bBold) const
{
    const int32 Requested = Size > 0 ? Size : GetDefault<UPGUIStyleSettings>()->BodyFontSize;
    const int32 Step = Requested <= 14 ? 14 : Requested <= 16 ? 16 : Requested <= 20 ? 20 : Requested <= 24 ? 24 : Requested <= 28 ? 28 : Requested;
    static const TSharedPtr<const FCompositeFont> Typeface = []
    {
        auto Font = MakeShared<FCompositeFont>();
        const FString Directory = FPaths::ProjectContentDir() / TEXT("UI/Fonts");
        Font->DefaultTypeface.AppendFont(TEXT("Regular"),Directory / TEXT("IBMPlexSansKR-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        Font->DefaultTypeface.AppendFont(TEXT("Bold"),Directory / TEXT("IBMPlexSansKR-SemiBold.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        return Font;
    }();
    return FSlateFontInfo(Typeface,Step,bBold ? TEXT("Bold") : TEXT("Regular"));
}

FLinearColor FPGUIStyle::RarityColor(EPGItemRarity Rarity) const
{
    switch (Rarity)
    {
    case EPGItemRarity::Epic: return FLinearColor(.65f,.25f,.8f);
    case EPGItemRarity::Legendary: return FLinearColor(1.f,.42f,.1f);
    case EPGItemRarity::Unique: return GetDefault<UPGUIStyleSettings>()->Unique;
    case EPGItemRarity::MythicUnique: return FLinearColor(.9f,.13f,.25f);
    default: break;
    }
    return Rarity == EPGItemRarity::Rare ? Rare : Rarity == EPGItemRarity::Magic ? Magic : Muted;
}
