#include "PGUIShadingComparison.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void UPGUIShadingComparison::SetLightState(bool bAvailable, float Azimuth, float Elevation, bool bOrbiting)
{
    LightLabel = FText::FromString(bAvailable ? FString::Printf(
        TEXT("주광원 · 방향 %+.0f° / 높이 %+.0f° · 자동 회전 %s"), Azimuth, Elevation,
        bOrbiting ? TEXT("켜짐") : TEXT("꺼짐")) : TEXT("비교 주광원을 연결해 주세요"));
}

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
                    .Text(FText::FromString(TEXT("1–8 단계 선택   ·   F 얼굴   ·   C 쿼터뷰   ·   0 / R 전체 보기")))]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                    .Text_Lambda([this]() { return FText::FromString(FString::Printf(
                        TEXT("J 머리카락 그림자: %s   ·   H 가림막 그림자: %s   ·   6 / 8단계에서 비교"),
                        bHairShadowEnabled ? TEXT("켜짐") : TEXT("꺼짐"),
                        bShadowEnabled ? TEXT("켜짐") : TEXT("꺼짐"))); })]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5, 0, 0)
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                    .Text(FText::FromString(TEXT("← / → 조명 좌우   ·   ↑ / ↓ 조명 높이   ·   Z 정면 / X 측면 / V 역광")))]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                    .Text(FText::FromString(TEXT("L 자동 회전   ·   Backspace 조명 초기화   ·   방향키 / 프리셋으로 자동 회전 정지")))]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                    .ColorAndOpacity(FLinearColor(1.f, .85f, .55f))
                    .Text_Lambda([this]() { return LightLabel; })]]]];
}
