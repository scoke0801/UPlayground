#include "PGUIMainHUD.h"
#include "PGCombatHUDStyle.h"
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
    const auto& Art=FPGCombatHUDStyle::Get();
    const auto& Fonts=FPGUIStyle::Get();
    return SNew(SBox).WidthOverride(300).Visibility(EVisibility::HitTestInvisible)
    [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)
            [SNew(STextBlock).Text_Lambda([this](){return FText::FromString(TEXT("달빛의 시련  /  ")+StageTitle.ToString());})
                .Font(Fonts.Font(16,true)).ColorAndOpacity(Art.Ivory).ShadowOffset(FVector2D(1,2))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(STextBlock).Text_Lambda([this](){return StagePhase;}).Font(Fonts.Font(11)).ColorAndOpacity(Art.Ivory).ShadowOffset(FVector2D(1,1))]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,9,0,9)
        [SNew(SBox).HeightOverride(1)[SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(Art.Gold)]]
        + SVerticalBox::Slot().AutoHeight()
        [SNew(STextBlock).Text_Lambda([this](){return Objective;}).Font(Fonts.Font(12)).ColorAndOpacity(Art.Ivory)
            .ShadowOffset(FVector2D(1,2)).WrapTextAt(300)]
    ];
}

TSharedRef<SWidget> UPGUIMainHUD::MakeActions()
{
    const auto& Art=FPGCombatHUDStyle::Get();
    const auto& Fonts=FPGUIStyle::Get();
    return SNew(SBox).WidthOverride(300).HAlign(HAlign_Right)
    [SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [SNew(SButton).Tag(TEXT("PGStartAction")).ButtonStyle(&ActionStyle).IsFocusable(false).ContentPadding(FMargin(38,10)).HAlign(HAlign_Center)
            .Visibility_Lambda([this](){return Stage.IsValid() && Stage->IsManualReady() && Stage->CanReady() ? EVisibility::Visible : EVisibility::Collapsed;})
            .OnClicked_Lambda([this](){if(Stage.IsValid()) Stage->ReadyForNextStage(); return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("시련 시작  ›"))).Font(Fonts.Font(14,true)).ColorAndOpacity(Art.Ivory)]]
        + SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)
        [SNew(SButton).Tag(TEXT("PGPrepareAction")).ButtonStyle(&ActionStyle).IsFocusable(false).ContentPadding(FMargin(12,10))
            .ToolTipText(FText::FromString(TEXT("장비와 검술 정비 · I")))
            .OnClicked_Lambda([this](){if(auto* PC=Cast<APGPlayerController>(GetOwningPlayer())) PC->ToggleInventory(); return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("장비  I"))).Font(Fonts.Font(12,true)).ColorAndOpacity(Art.Ivory)]]
    ];
}

TSharedRef<SWidget> UPGUIMainHUD::MakeBuildPanel()
{
    const auto& Art=FPGCombatHUDStyle::Get();
    const auto& Fonts=FPGUIStyle::Get();
    auto Families=SNew(SHorizontalBox);
    const TCHAR* Names[]={TEXT("출혈"),TEXT("충격파"),TEXT("격분")};
    const FLinearColor Colors[]={FLinearColor(.9f,.22f,.19f),FLinearColor(.56f,.64f,.70f),FLinearColor(.9f,.56f,.17f)};
    for(int32 I=0;I<3;++I)
        Families->AddSlot().AutoWidth().Padding(3,0)
        [SNew(SBorder).BorderImage(&Art.Panel).Padding(FMargin(9,4))
            .Visibility_Lambda([this,I](){return BuildActive[I] ? EVisibility::HitTestInvisible : EVisibility::Collapsed;})
            [SNew(STextBlock).Font(Fonts.Font(11,true)).ColorAndOpacity(Colors[I])
                .Text_Lambda([this,I,Name=FString(Names[I])](){return FText::FromString(Name+ (BuildCore[I] ? TEXT("핵심") : FString::Printf(TEXT(" %d/3"),BuildBranches[I])));})]];
    return SNew(SVerticalBox).Visibility_Lambda([this](){return bRogueHUD ? EVisibility::HitTestInvisible : EVisibility::Collapsed;})
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Families]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,4,0,0)
        [SNew(SBox).WidthOverride(820)
            [SNew(STextBlock).Font(Fonts.Font(11)).ColorAndOpacity(Art.Ivory).ShadowOffset(FVector2D(1,1))
                .Justification(ETextJustify::Center).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                .Text_Lambda([this](){FString Text=BuildProc.IsEmpty() ? BuildStatus.ToString() : BuildProc.ToString(); Text.ReplaceInline(TEXT("\n"),TEXT("  ·  ")); return FText::FromString(Text);})]];
}
