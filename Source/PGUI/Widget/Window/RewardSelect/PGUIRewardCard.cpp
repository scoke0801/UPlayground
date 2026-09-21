#include "PGUIRewardCard.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UPGUIRewardCard::Configure(int32 InIndex, FText InTitle, FText InDescription, EPGRewardGrade InGrade, UTexture2D* InIcon, int32 InIconPanel)
{
    Index = InIndex; Title = InTitle; Description = InDescription; Icon = InIcon; IconPanel = InIconPanel; SetGrade(InGrade);
}
void UPGUIRewardCard::SetGrade(EPGRewardGrade Grade)
{
    GradeColor = Grade == EPGRewardGrade::Rare ? FLinearColor(1.f,.65f,.15f) : Grade == EPGRewardGrade::Magic ? FLinearColor(.2f,.65f,1.f) : FLinearColor(.65f,.7f,.75f);
    if (Frame) Frame->SetBrushColor(GradeColor);
}
TSharedRef<SWidget> UPGUIRewardCard::RebuildWidget()
{
    if (!WidgetTree->RootWidget)
    {
        auto* Size = WidgetTree->ConstructWidget<USizeBox>();
        Size->SetWidthOverride(300); Size->SetHeightOverride(550); WidgetTree->RootWidget = Size;
        Frame = WidgetTree->ConstructWidget<UBorder>(); Frame->SetPadding(FMargin(1.5f)); Frame->SetBrushColor(GradeColor); Size->SetContent(Frame);
        Button = WidgetTree->ConstructWidget<UButton>(); Frame->SetContent(Button);
        Button->SetBackgroundColor(FLinearColor(.035f,.04f,.055f));
        auto* ContentBorder = WidgetTree->ConstructWidget<UBorder>(); ContentBorder->SetBrushColor(FLinearColor(.016f,.019f,.026f)); ContentBorder->SetPadding(FMargin(18,16)); Button->SetContent(ContentBorder);
        auto* List = WidgetTree->ConstructWidget<UVerticalBox>(); ContentBorder->SetContent(List);
        auto* Eyebrow = WidgetTree->ConstructWidget<UTextBlock>();
        Eyebrow->SetText(NSLOCTEXT("PG", "RunBoon", "런 특성")); Eyebrow->SetColorAndOpacity(GradeColor);
        auto SmallFont = Eyebrow->GetFont(); SmallFont.Size = 12; Eyebrow->SetFont(SmallFont); Eyebrow->SetJustification(ETextJustify::Center); List->AddChild(Eyebrow);
        auto* PictureSize = WidgetTree->ConstructWidget<USizeBox>(); PictureSize->SetWidthOverride(250); PictureSize->SetHeightOverride(250);
        auto* Picture = WidgetTree->ConstructWidget<UImage>(); Picture->SetBrushFromTexture(Icon);
        if (IconPanel >= 0 && IconPanel < 3)
        {
            FSlateBrush Brush = Picture->GetBrush();
            Brush.SetUVRegion(FBox2f(FVector2f(IconPanel / 3.f, 0), FVector2f((IconPanel + 1) / 3.f, 1)));
            Picture->SetBrush(Brush);
        }
        PictureSize->SetContent(Picture);
        List->AddChildToVerticalBox(PictureSize)->SetHorizontalAlignment(HAlign_Center);
        auto* Name = WidgetTree->ConstructWidget<UTextBlock>(); Name->SetText(Title); Name->SetJustification(ETextJustify::Center); Name->SetColorAndOpacity(GradeColor); Name->SetAutoWrapText(true); auto NameFont = Name->GetFont(); NameFont.Size = 24; Name->SetFont(NameFont); List->AddChildToVerticalBox(Name)->SetPadding(FMargin(0,10,0,12));
        auto* Desc = WidgetTree->ConstructWidget<UTextBlock>(); Desc->SetText(Description); Desc->SetJustification(ETextJustify::Center); Desc->SetAutoWrapText(true); auto DescFont = Desc->GetFont(); DescFont.Size = 16; Desc->SetFont(DescFont); Desc->SetColorAndOpacity(FLinearColor(.8f,.82f,.86f)); List->AddChild(Desc);
        Button->OnClicked.AddDynamic(this, &ThisClass::Clicked);
        Button->OnHovered.AddDynamic(this, &ThisClass::Hovered);
        Button->OnUnhovered.AddDynamic(this, &ThisClass::Unhovered);
    }
    return Super::RebuildWidget();
}
void UPGUIRewardCard::Clicked() { OnSelected.ExecuteIfBound(Index); }
void UPGUIRewardCard::Hovered() { SetRenderScale(FVector2D(1.04)); }
void UPGUIRewardCard::Unhovered() { SetRenderScale(FVector2D(1)); }
void UPGUIRewardCard::SetConfirmed(bool bSelected)
{
    SetIsEnabled(false); SetRenderOpacity(bSelected ? 1.f : .25f); SetRenderScale(FVector2D(bSelected ? 1.06f : .96f));
}
