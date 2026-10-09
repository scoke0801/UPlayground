#include "PGUIMainHUD.h"
#include "PGCombatHUDStyle.h"
#include "PGUI/Style/PGUIStyle.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Components/Combat/PGConsumableComponent.h"
#include "PGData/DataAsset/Input/DataAsset_InputConfig.h"
#include "PGShared/Shared/Tag/PGGamePlayInputTags.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"

/** Authored anime bottle with a runtime cooldown arc and a vector missing-art fallback. */
class SPGHealingBottle : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPGHealingBottle) : _Icon(nullptr) {}
        SLATE_ARGUMENT(const FSlateBrush*, Icon)
        SLATE_ATTRIBUTE(float, Remaining)
        SLATE_ATTRIBUTE(bool, Highlight)
        SLATE_ATTRIBUTE(bool, Empty)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Icon=Args._Icon; Remaining=Args._Remaining; Highlight=Args._Highlight; Empty=Args._Empty; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(42,46); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
        int32 Layer, const FWidgetStyle& WidgetStyle, bool) const override
    {
        const FVector2D Size=G.GetLocalSize(), Center=Size*.5;
        const FLinearColor Mint=Empty.Get() ? FLinearColor(.25f,.3f,.35f) : FLinearColor(.65f,.028f,.038f);
        auto Box=[&](FVector2D Pos,FVector2D Extent,const FSlateBrush* Brush,FLinearColor Color)
        { FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(Extent,FSlateLayoutTransform(Pos)),Brush,ESlateDrawEffect::None,Color); };
        if(Icon && Icon->GetResourceObject())
        {
            const FLinearColor Tint=Empty.Get() ? FLinearColor(.30f,.30f,.34f,.6f) : FLinearColor::White;
            FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(),Icon,ESlateDrawEffect::None,Tint*WidgetStyle.GetColorAndOpacityTint());
        }
        else
        {
        static const FSlateRoundedBoxBrush Body(FLinearColor::White,5.f);
        static const FSlateRoundedBoxBrush Neck(FLinearColor::White,2.f);
        Box(Center+FVector2D(-10,-7),FVector2D(20,23),&Body,FLinearColor(.055f,.025f,.020f));
        Box(Center+FVector2D(-8,0),FVector2D(16,13),&Body,Mint.CopyWithNewOpacity(.8f));
        Box(Center+FVector2D(-5,-14),FVector2D(10,10),&Neck,Mint);
        Box(Center+FVector2D(-7,-16),FVector2D(14,4),&Neck,FPGCombatHUDStyle::Get().Gold);
        Box(Center+FVector2D(-2,1),FVector2D(4,10),&Neck,FLinearColor::White);
        Box(Center+FVector2D(-5,4),FVector2D(10,4),&Neck,FLinearColor::White);
        }
        TArray<FVector2D> Points;
        const float Fraction=Remaining.Get();
        const float Arc=Fraction>0 ? FMath::Clamp(Fraction,0.f,1.f) : 1.f;
        for(int32 I=0;I<=48;++I)
        {
            const float Angle=-HALF_PI+2.f*PI*Arc*I/48.f;
            Points.Add(Center+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*20.f);
        }
        FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,
            Fraction>0 ? FPGCombatHUDStyle::Get().Gold : Highlight.Get() ? Mint : Mint.CopyWithNewOpacity(.22f),true,Highlight.Get()?2.5f:1.5f);
        return Layer+2;
    }
private:
    const FSlateBrush* Icon=nullptr;
    TAttribute<float> Remaining;
    TAttribute<bool> Highlight, Empty;
};

TSharedRef<SWidget> UPGUIMainHUD::MakeHealingPotion()
{
    return SNew(SBox).WidthOverride(56).HeightOverride(70)
    [SNew(SButton).Tag(TEXT("PGHealingPotion")).ButtonStyle(&FPGCombatHUDStyle::Get().Button).IsFocusable(false).ContentPadding(3)
        .ToolTipText_Lambda([this](){return FText::FromString(FString::Printf(TEXT("%s\n최대 체력의 %.0f%% 즉시 회복 · 재사용 %.0f초\n구간 클리어 시 보충\n%s"),
            *Potion.Name.ToString(),Potion.HealFraction*100,Potion.CooldownDuration,*Potion.Reason.ToString()));})
        .OnClicked_Lambda([this](){if(auto* P=Cast<APGCharacterPlayer>(GetOwningPlayerPawn())) P->GetConsumableComponent()->TryUse();return FReply::Handled();})
        [SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top)
            [SNew(SPGHealingBottle).Icon(&PotionIconBrush).Remaining_Lambda([this](){return Potion.CooldownDuration>0 ? Potion.Cooldown/Potion.CooldownDuration : 0.f;})
                .Highlight_Lambda([this](){return Potion.bCanUse && HealthRatio<=Potion.LowHealthFraction;})
                .Empty_Lambda([this](){return Potion.Count<=0;})]
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
            [SNew(STextBlock).Text_Lambda([this](){return PotionKey;}).Font(FCoreStyle::GetDefaultFontStyle("Bold",10))]
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top)
            [SNew(STextBlock).Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("%d"),Potion.Count));})
                .ColorAndOpacity(FPGCombatHUDStyle::Get().Ivory).Font(FCoreStyle::GetDefaultFontStyle("Bold",13))]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)
            [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",10)).ColorAndOpacity(FPGCombatHUDStyle::Get().Ivory)
                .Text_Lambda([this](){return FText::FromString(Potion.Count==0 ? TEXT("소진") :
                    Potion.Cooldown>0 ? FString::Printf(TEXT("%.1f초"),Potion.Cooldown) : FString::Printf(TEXT("%d/%d"),Potion.Count,Potion.Capacity));})]]];
}

void UPGUIMainHUD::RefreshHealingPotion()
{
    const auto* Player=Cast<APGCharacterPlayer>(GetOwningPlayerPawn());
    Potion=Player ? Player->GetConsumableComponent()->GetState() : FPGConsumableState();
    PotionKey=FText::FromString(TEXT("—"));
    if(Player && Player->GetInputConfig() && GetOwningLocalPlayer())
        if(auto* Input=GetOwningLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            if(const auto* Action=Player->GetInputConfig()->FindNativeInputActionsByTag(PGGamePlayTags::InputTag_HealingPotion))
            {
                const auto Keys=Input->QueryKeysMappedToAction(Action);
                if(!Keys.IsEmpty()) PotionKey=Keys[0].GetDisplayName(false);
            }
    const double Now=GetWorld()->GetTimeSeconds();
    if(!bPotionHintShown && Potion.bCanUse && HealthRatio<=Potion.LowHealthFraction)
    { bPotionHintShown=true; PotionHintUntil=Now+6.; }
    PotionNotice=Potion.Notice;
    if(PotionNotice.IsEmpty() && Now<PotionHintUntil && Potion.bCanUse)
        PotionNotice=FText::FromString(PotionKey.ToString()+TEXT(" 회복약 사용"));
}

void UPGUIMainHUD::OnConsumableChanged(const IPGEventData* Event)
{
    if(Event && static_cast<const FPGConsumablePresentation*>(Event)->Owner==GetOwningPlayerPawn()) Refresh();
}
