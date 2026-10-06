#include "PGUIShadingComparison.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UPGUIShadingComparison::RebuildWidget()
{
    SetIsFocusable(false);
    return SNew(SSafeZone).Visibility(EVisibility::HitTestInvisible)
    [SNew(SOverlay)
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(16, 16)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(.015f, .025f, .04f, .9f)).Padding(18, 10)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                    .ColorAndOpacity(FLinearColor(.65f, .85f, 1.f))
                    .Text_Lambda([this]() { return ViewLabel; })]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 5).HAlign(HAlign_Center)
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                    .Text(FText::FromString(TEXT("WASD 이동   ·   Q / E 하강 / 상승   ·   우클릭 + 마우스 회전   ·   Shift 빠르게")))]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                    .Text(FText::FromString(TEXT("1–5 단계 선택   ·   F 얼굴   ·   C 쿼터뷰   ·   0 / R 전체 보기")))]]]];
}
