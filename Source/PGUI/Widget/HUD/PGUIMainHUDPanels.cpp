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
        [SNew(SButton).Tag(TEXT("PGStartAction")).ButtonStyle(&ActionStyle).IsFocusable(false).ContentPadding(FMargin(16,10)).HAlign(HAlign_Center)
            .Visibility_Lambda([this](){return Stage.IsValid() && Stage->IsManualReady() && Stage->CanReady() ? EVisibility::Visible : EVisibility::Collapsed;})
            .OnClicked_Lambda([this](){if(Stage.IsValid()) Stage->ReadyForNextStage(); return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("시련 시작  ›"))).Font(Fonts.Font(14,true)).ColorAndOpacity(Art.Ivory)]]
        + SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)
        [SNew(SButton).Tag(TEXT("PGPrepareAction")).ButtonStyle(&ActionStyle).IsFocusable(false).ContentPadding(FMargin(12,10))
            .ToolTipText(FText::FromString(TEXT("장비와 검술 정비 · I")))
            .OnClicked_Lambda([this](){if(auto* PC=Cast<APGPlayerController>(GetOwningPlayer())) PC->ToggleInventory(); return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("장비  I"))).Font(Fonts.Font(12,true)).ColorAndOpacity(Art.Ivory)]]
        + SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)
        [SNew(SButton).Tag(TEXT("PGSettingsAction")).ButtonStyle(&ActionStyle).IsFocusable(false).ContentPadding(FMargin(10,10))
            .ToolTipText(FText::FromString(TEXT("카메라 모드 설정 · Esc")))
            .OnClicked_Lambda([this](){if(auto* PC=Cast<APGPlayerController>(GetOwningPlayer())) PC->ToggleSettings(); return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("설정"))).Font(Fonts.Font(12,true)).ColorAndOpacity(Art.Ivory)]]
    ];
}
