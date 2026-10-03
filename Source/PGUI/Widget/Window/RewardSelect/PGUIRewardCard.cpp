#include "PGUIRewardCard.h"
#include "PGUI/Style/PGUIStyle.h"
#include "Engine/Texture2D.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

void UPGUIRewardCard::Configure(int32 InIndex, FText InTitle, FText InDescription, EPGRewardGrade InGrade, UTexture2D* InIcon, int32 InIconPanel)
{
    Index = InIndex; Title = InTitle; Description = InDescription; Icon = InIcon; IconPanel = InIconPanel; SetGrade(InGrade);
}
void UPGUIRewardCard::SetGrade(EPGRewardGrade Grade)
{
    const auto& Style = FPGUIStyle::Get();
    GradeColor = Grade == EPGRewardGrade::Rare ? Style.Rare : Grade == EPGRewardGrade::Magic ? Style.Magic : Style.Muted;
}
TSharedRef<SWidget> UPGUIRewardCard::RebuildWidget()
{
    const auto& Style = FPGUIStyle::Get();
    IconBrush.SetResourceObject(Icon); IconBrush.ImageSize = FVector2D(96);
    if (IconPanel >= 0 && IconPanel < 3)
        IconBrush.SetUVRegion(FBox2f(FVector2f(IconPanel / 3.f, 0), FVector2f((IconPanel + 1) / 3.f, 1)));
    return SNew(SBorder).BorderImage_Lambda([this](){return bConfirmed ? &FPGUIStyle::Get().Selected : &FPGUIStyle::Get().Card;}).Padding(20)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [SNew(STextBlock).Text(Context).Font(Style.Font(16,true)).ColorAndOpacity(GradeColor).AutoWrapText(true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,18,0,14).HAlign(HAlign_Center)
            [SNew(SBox).WidthOverride(96).HeightOverride(96).Visibility(Icon ? EVisibility::Visible : EVisibility::Collapsed)
                [SNew(SImage).Image(&IconBrush)]]
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
            [SNew(STextBlock).Text(Title).Font(Style.Font(24,true)).ColorAndOpacity(Style.Text).AutoWrapText(true)]
            + SVerticalBox::Slot().FillHeight(1)
            [SNew(SScrollBox) + SScrollBox::Slot()
                [SNew(STextBlock).Text(Description).Font(Style.Font(18)).ColorAndOpacity(Style.Muted).AutoWrapText(true)]]
            + SVerticalBox::Slot().AutoHeight().Padding(0,18,0,0)
            [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(FMargin(16,12)).HAlign(HAlign_Center)
                .OnClicked_Lambda([this](){OnSelected.ExecuteIfBound(Index); return FReply::Handled();})
                [SNew(STextBlock).Text_Lambda([this](){return FText::FromString(bConfirmed ? TEXT("선택 적용 중…") : bContinueOnly ? TEXT("계속") : TEXT("이 강화 선택"));})
                    .Font(Style.Font(18,true)).ColorAndOpacity(Style.Mint)]]];
}
void UPGUIRewardCard::SetConfirmed(bool bSelected)
{
    bConfirmed = bSelected;
    SetIsEnabled(false); SetRenderOpacity(bSelected ? 1.f : .35f);
}
void UPGUIRewardCard::ResetConfirmation()
{
    bConfirmed = false; SetIsEnabled(true); SetRenderOpacity(1); SetRenderScale(FVector2D(1));
}
