#include "PGUIMainHUD.h"
#include "PGUI/Style/PGUIStyle.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGActor/Manager/PGStageManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UPGUIMainHUD::MakeStagePanel()
{
    const auto& Style = FPGUIStyle::Get();
    return SNew(SBox).WidthOverride(370).Visibility(EVisibility::HitTestInvisible)
        [SNew(SBorder).BorderImage(&PlaqueBrush).Padding(FMargin(32,20,24,20))
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("달빛의 시련"))).Font(Style.Font(12,true)).ColorAndOpacity(Style.Muted)]
                    + SHorizontalBox::Slot().AutoWidth()
                    [SNew(STextBlock).Text_Lambda([this](){return StagePhase;}).Font(Style.Font(12,true)).ColorAndOpacity(Style.Mint)]]
                + SVerticalBox::Slot().AutoHeight().Padding(0,6,0,10)
                [SNew(STextBlock).Text_Lambda([this](){return StageTitle;}).Font(Style.Font(28,true)).ColorAndOpacity(Style.Text)]
                + SVerticalBox::Slot().AutoHeight()
                [SNew(STextBlock).Text_Lambda([this](){return Objective;}).Font(Style.Font(14)).ColorAndOpacity(Style.Text).WrapTextAt(306)]]];
}

TSharedRef<SWidget> UPGUIMainHUD::MakeActions()
{
    const auto& Style = FPGUIStyle::Get();
    return SNew(SBox).WidthOverride(230)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [SNew(SButton).Tag(TEXT("PGPrepareAction")).ButtonStyle(&ActionStyle).IsFocusable(false).ContentPadding(FMargin(26,17))
                .OnClicked_Lambda([this](){if(auto* PC=Cast<APGPlayerController>(GetOwningPlayer())) PC->ToggleInventory(); return FReply::Handled();})
                [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("전투 준비"))).Font(Style.Font(16,true)).ColorAndOpacity(Style.Text)]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("I"))).Font(Style.Font(13,true)).ColorAndOpacity(Style.Muted)]]]
            + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
            [SNew(SButton).Tag(TEXT("PGStartAction")).ButtonStyle(&ActionStyle).IsFocusable(false).ContentPadding(FMargin(26,19)).HAlign(HAlign_Center)
                .Visibility_Lambda([this](){return Stage.IsValid() && Stage->IsManualReady() && Stage->CanReady() ? EVisibility::Visible : EVisibility::Collapsed;})
                .OnClicked_Lambda([this](){if(Stage.IsValid()) Stage->ReadyForNextStage(); return FReply::Handled();})
                [SNew(STextBlock).Text(FText::FromString(TEXT("시련 시작  ›"))).Font(Style.Font(19,true)).ColorAndOpacity(Style.Mint)]]];
}

TSharedRef<SWidget> UPGUIMainHUD::MakeBuildPanel()
{
    const auto& Style = FPGUIStyle::Get();
    auto Families = SNew(SHorizontalBox);
    const TCHAR* Names[] = {TEXT("출혈"),TEXT("충격파"),TEXT("격분")};
    const FLinearColor Colors[] = {Style.Danger,Style.Magic,Style.Rare};
    for (int32 I=0; I<3; ++I)
    {
        auto Progress = SNew(SHorizontalBox);
        for (int32 N=0; N<3; ++N)
            Progress->AddSlot().FillWidth(1).Padding(1,0)
                [SNew(SBox).HeightOverride(3)
                    [SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .ColorAndOpacity_Lambda([this,I,N,Color=Colors[I]](){return BuildBranches[I]>N ? Color : FLinearColor(.045f,.06f,.09f);})]];
        Families->AddSlot().FillWidth(1).Padding(I==0 ? 0 : 10,0,0,0)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [SNew(STextBlock).Text(FText::FromString(Names[I])).Font(Style.Font(14,true))
                    .ColorAndOpacity_Lambda([this,I,Color=Colors[I]](){return BuildActive[I] ? Color : FPGUIStyle::Get().Muted;})]
                + SVerticalBox::Slot().AutoHeight().Padding(0,6,0,7)[Progress]
                + SVerticalBox::Slot().AutoHeight()
                [SNew(STextBlock).Font(Style.Font(11)).ColorAndOpacity(Style.Muted)
                    .Text_Lambda([this,I](){return FText::FromString(BuildCore[I] ? TEXT("핵심 완성") : BuildActive[I] ? FString::Printf(TEXT("강화 %d/3"),BuildBranches[I]) : TEXT("미획득"));})]];
    }
    return SNew(SBox).WidthOverride(320).Visibility_Lambda([this](){return bRogueHUD ? EVisibility::HitTestInvisible : EVisibility::Collapsed;})
        [SNew(SBorder).BorderImage(&PlaqueBrush).Padding(FMargin(28,20,22,20))
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
                [SNew(STextBlock).Text(FText::FromString(TEXT("강화 공명"))).Font(Style.Font(15,true)).ColorAndOpacity(Style.Lavender)]
                + SVerticalBox::Slot().AutoHeight()[Families]
                + SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
                [SNew(STextBlock).Text_Lambda([this](){return BuildSummary;}).Font(Style.Font(12)).ColorAndOpacity(Style.Muted).WrapTextAt(270)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
                [SNew(STextBlock).Visibility_Lambda([this](){return BuildStatus.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;})
                    .Text_Lambda([this](){return BuildStatus;}).Font(Style.Font(12)).ColorAndOpacity(Style.Text).WrapTextAt(270)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)
                [SNew(STextBlock).Visibility_Lambda([this](){return BuildProc.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;})
                    .Text_Lambda([this](){return BuildProc;}).Font(Style.Font(12,true)).ColorAndOpacity(Style.Mint).WrapTextAt(270)]]];
}
