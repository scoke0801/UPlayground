#include "PGUISettings.h"
#include "PGActor/Controllers/PGPlayerController.h"
#include "PGData/DataAsset/Input/PGCameraSettings.h"
#include "PGUI/Manager/PGUIManager.h"
#include "PGUI/Style/PGUIStyle.h"
#include "PGUI/Widget/HUD/PGCombatHUDStyle.h"
#include "InputCoreTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

UPGUISettings::UPGUISettings(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetIsFocusable(true);
}

TSharedRef<SWidget> UPGUISettings::RebuildWidget()
{
    const auto& Style = FPGCombatHUDStyle::Get();
    const auto& Fonts = FPGUIStyle::Get();
    const auto ModeButton = [this, &Style, &Fonts](EPGCameraMode Mode, const TCHAR* Title, const TCHAR* Description) -> TSharedRef<SWidget>
    {
        const FString Label(Title);
        return SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(18)
            .Tag(Mode == EPGCameraMode::QuarterView ? TEXT("PGCameraQuarterView") : TEXT("PGCameraAction3D"))
            .OnClicked_Lambda([this, Mode]()
            {
                if (auto* PC = Cast<APGPlayerController>(GetOwningPlayer())) PC->SetPreferredCameraMode(Mode);
                return FReply::Handled();
            })
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [SNew(STextBlock).Font(Fonts.Font(18, true)).ColorAndOpacity(Style.Ivory)
                    .Text_Lambda([Mode, Label]()
                    {
                        return FText::FromString(Label + (GetDefault<UPGCameraSettings>()->GetCameraMode() == Mode ? TEXT("  ·  선택됨") : TEXT("")));
                    })]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                [SNew(STextBlock).Text(FText::FromString(Description)).Font(Fonts.Font(13)).ColorAndOpacity(Style.Ivory).WrapTextAt(500)]
            ];
    };
    return SNew(SBorder).BorderImage(&Style.Shade).Padding(24).HAlign(HAlign_Center).VAlign(VAlign_Center)
    [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
        [SNew(SBox).WidthOverride(600)
            [SNew(SBorder).BorderImage(&Style.Panel).Padding(28)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Text(FText::FromString(TEXT("설정"))).Font(Fonts.Font(26, true)).ColorAndOpacity(Style.Ivory)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 12)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("카메라 모드"))).Font(Fonts.Font(16, true)).ColorAndOpacity(Style.Ivory)]
                    + SVerticalBox::Slot().AutoHeight()
                    [ModeButton(EPGCameraMode::QuarterView, TEXT("쿼터뷰 · 기본"), TEXT("좌우 방향 고정 · 휠로 확대/축소\n가운데 버튼을 누르고 위아래로 드래그하면 높이 각도를 조절합니다."))]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
                    [ModeButton(EPGCameraMode::Action3D, TEXT("3D 액션"), TEXT("마우스로 상하·좌우 회전 · 휠로 확대/축소\nAlt를 누르면 커서를 표시해 화면의 버튼을 사용할 수 있습니다."))]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 20)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("선택 즉시 적용되며 다음 실행에도 유지됩니다."))).Font(Fonts.Font(12)).ColorAndOpacity(Style.Ivory)]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                    [SNew(SButton).ButtonStyle(&Style.Button).ContentPadding(FMargin(24, 10)).Tag(TEXT("PGSettingsClose"))
                        .OnClicked_Lambda([this]() { if (auto* PC = Cast<APGPlayerController>(GetOwningPlayer())) PC->CloseSettings(); return FReply::Handled(); })
                        [SNew(STextBlock).Text(FText::FromString(TEXT("닫기  Esc"))).Font(Fonts.Font(14, true)).ColorAndOpacity(Style.Ivory)]]
                ]
            ]
        ]
    ];
}

FReply UPGUISettings::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape)
    {
        if (auto* PC = Cast<APGPlayerController>(GetOwningPlayer())) PC->CloseSettings();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UPGUISettings::NativeDestruct()
{
    if (auto* UI = UPGUIManager::Get(this)) UI->ReleaseModalInput(this);
    Super::NativeDestruct();
}
