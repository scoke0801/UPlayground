#include "PGUILootLabel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
void UPGUILootLabel::Configure(const FText& Name, FLinearColor Color, UTexture2D* Texture)
{
    ItemName = Name; GradeColor = Color; Icon = Texture;
}
TSharedRef<SWidget> UPGUILootLabel::RebuildWidget()
{
    auto* Frame = WidgetTree->ConstructWidget<UBorder>(); Frame->SetBrushColor(GradeColor); Frame->SetPadding(FMargin(2)); WidgetTree->RootWidget = Frame;
    auto* Back = WidgetTree->ConstructWidget<UBorder>(); Back->SetBrushColor(FLinearColor(.015f,.02f,.03f,.93f)); Back->SetPadding(FMargin(6,3)); Frame->SetContent(Back);
    auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>(); Back->SetContent(Row);
    auto* Image = WidgetTree->ConstructWidget<UImage>(); Image->SetBrushFromTexture(Icon); Image->SetDesiredSizeOverride(FVector2D(28,28)); Row->AddChild(Image);
    auto* Text = WidgetTree->ConstructWidget<UTextBlock>(); Text->SetText(FText::Format(NSLOCTEXT("PG", "LootLabel", "{0}  [E]"), ItemName)); Text->SetColorAndOpacity(GradeColor);
    auto Font = Text->GetFont(); Font.Size = 15; Text->SetFont(Font); Row->AddChildToHorizontalBox(Text)->SetPadding(FMargin(6,0));
    return Super::RebuildWidget();
}
