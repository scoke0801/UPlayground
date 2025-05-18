// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/PGMarqueeEditableTextBox.h"
#include "Widgets/Text/STextScroller.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Framework/Application/SlateApplication.h"
#include "CommonTextBlock.h"
#include "Misc/CoreDelegates.h"
#include "Input/Events.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"

//////////////////////////////////////////////////
// SPGMarqueeEditableTextBox
//////////////////////////////////////////////////

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::Construct(const FArguments& InArgs)
{
    ScrollState = EScrollState::Idle;
    
    // 부모 EditableTextBox에서 스타일 가져오기 - 특정 스타일을 강제할 필요 없음
    // 기본 스타일링을 사용하여 편집 가능한 텍스트 위젯 생성
    EditableText = SNew(SEditableText)
        .Text(InArgs._Text)
        .HintText(InArgs._HintText)
        .Font(InArgs._Font)  // 폰트 속성 적용
        .IsReadOnly(InArgs._IsReadOnly)
        .ColorAndOpacity(InArgs._ColorAndOpacity)
        .IsPassword(InArgs._IsPassword)
        .MinDesiredWidth(InArgs._MinDesiredWidth)
        .IsCaretMovedWhenGainFocus(InArgs._IsCaretMovedWhenGainFocus)
        .SelectAllTextWhenFocused(InArgs._SelectAllTextWhenFocused)
        .RevertTextOnEscape(InArgs._RevertTextOnEscape)
        .ClearKeyboardFocusOnCommit(InArgs._ClearKeyboardFocusOnCommit)
        .SelectAllTextOnCommit(InArgs._SelectAllTextOnCommit)
        .AllowContextMenu(InArgs._AllowContextMenu)
        .OnTextChanged(InArgs._OnTextChanged)
        .OnTextCommitted(InArgs._OnTextCommitted);
    
    // 일반 텍스트를 위한 텍스트 스크롤러 생성
    TextScroller = SNew(STextScroller)
        .ScrollOptions(InArgs._TextScrollOptions ? *InArgs._TextScrollOptions : FTextScrollerOptions())
        [
            EditableText.ToSharedRef()
        ];
    
    // 힌트 텍스트 표시를 위한 텍스트 블록 생성
    TSharedRef<STextBlock> HintTextBlock = SNew(STextBlock)
        .Text(InArgs._HintText)
        .Font(InArgs._Font)
        // 힌트 텍스트에 흐린 전경색 사용 - 시스템 스타일링을 존중함
        // 참고: 힌트 텍스트는 일반적으로 회색으로 표시되기 때문에 ColorAndOpacity를 직접 전달하지 않고
        // UseSubduedForeground()를 사용하여 현재 테마에 맞는 흐린 색상을 사용합니다.
        .ColorAndOpacity(FSlateColor::UseSubduedForeground());
    
    // 힌트 텍스트 스크롤러 생성
    HintTextScroller = SNew(STextScroller)
        .ScrollOptions(InArgs._HintTextScrollOptions ? *InArgs._HintTextScrollOptions : FTextScrollerOptions())
        [
            HintTextBlock
        ];
    
    // 두 스크롤러를 위한 컨테이너 생성
    TSharedRef<SWidget> ContentWidget = SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBox)
            .Visibility_Lambda([this]() {
                return !ShouldShowHintText() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
            })
            [
                TextScroller.ToSharedRef()
            ]
        ]
        + SOverlay::Slot()
        [
            SNew(SBox)
            .Visibility_Lambda([this]() {
                return ShouldShowHintText() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
            })
            [
                HintTextScroller.ToSharedRef()
            ]
        ];
    
    // 스크롤러를 포함할 테두리 생성
    Border = SNew(SBorder)
        .BorderImage(&InArgs._ScrollStyle->BackgroundImageNormal)
        .Padding(InArgs._TextMargin)
        [
            ContentWidget
        ];

    // 콘텐츠를 테두리로 설정
    ChildSlot
    [
        Border.ToSharedRef()
    ];
    
    // 스크롤 옵션에 따라 스크롤링 활성화 또는 비활성화
    if (InArgs._TextScrollOptions && InArgs._TextScrollOptions->Speed > 0.0f)
    {
        TextScroller->StartScrolling();
        ScrollState = EScrollState::Scrolling;
    }
    else
    {
        TextScroller->SuspendScrolling();
        ScrollState = EScrollState::Idle;
    }
    
    if (InArgs._HintTextScrollOptions && InArgs._HintTextScrollOptions->Speed > 0.0f)
    {
        HintTextScroller->StartScrolling();
    }
    else
    {
        HintTextScroller->SuspendScrolling();
    }
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::SetText(const TAttribute<FText>& InText)
{
    if (EditableText.IsValid())
    {
        EditableText->SetText(InText);
    }
}

FText UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::GetText() const
{
    if (EditableText.IsValid())
    {
        return EditableText->GetText();
    }
    return FText::GetEmpty();
}

FVector2D UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::GetDesiredSize() const
{
    return SCompoundWidget::GetDesiredSize();
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::SetScrollingEnabled(bool bEnabled)
{
    if (TextScroller.IsValid())
    {
        if (bEnabled && ScrollState != EScrollState::Paused)
        {
            TextScroller->StartScrolling();
            ScrollState = EScrollState::Scrolling;
        }
        else
        {
            TextScroller->SuspendScrolling();
            if (!bEnabled)
            {
                ScrollState = EScrollState::Idle;
            }
        }
    }
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::SetHintTextScrollingEnabled(bool bEnabled)
{
    if (HintTextScroller.IsValid())
    {
        if (bEnabled)
        {
            HintTextScroller->StartScrolling();
        }
        else
        {
            HintTextScroller->SuspendScrolling();
        }
    }
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::ResetScrollState()
{
    if (TextScroller.IsValid())
    {
        TextScroller->ResetScrollState();
        
        if (ScrollState != EScrollState::Paused && ScrollState != EScrollState::Idle)
        {
            TextScroller->StartScrolling();
            ScrollState = EScrollState::Scrolling;
        }
    }
    
    if (HintTextScroller.IsValid())
    {
        HintTextScroller->ResetScrollState();
        
        if (ScrollState != EScrollState::Paused && ScrollState != EScrollState::Idle)
        {
            HintTextScroller->StartScrolling();
        }
    }
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::PauseScrolling()
{
    if (ScrollState != EScrollState::Paused && TextScroller.IsValid())
    {
        TextScroller->SuspendScrolling();
        ScrollState = EScrollState::Paused;
    }
    
    if (HintTextScroller.IsValid())
    {
        HintTextScroller->SuspendScrolling();
    }
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::ResumeScrolling()
{
    if (ScrollState == EScrollState::Paused)
    {
        if (TextScroller.IsValid())
        {
            TextScroller->StartScrolling();
            ScrollState = EScrollState::Scrolling;
        }
        else
        {
            ScrollState = EScrollState::Idle;
        }
        
        if (HintTextScroller.IsValid())
        {
            HintTextScroller->StartScrolling();
        }
    }
}

bool UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::IsInputActive() const
{
    return EditableText.IsValid() && EditableText->HasKeyboardFocus();
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::SetFont(const FSlateFontInfo& InFont)
{
    if (EditableText.IsValid())
    {
        EditableText->SetFont(InFont);
    }
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::SetColorAndOpacity(const FSlateColor& InColorAndOpacity)
{
    if (EditableText.IsValid())
    {
        EditableText->SetColorAndOpacity(InColorAndOpacity);
    }
}

bool UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::ShouldShowHintText() const
{
    if (EditableText.IsValid())
    {
        // Show hint text if the editable text is empty and doesn't have focus
        return EditableText->GetText().IsEmpty() && !EditableText->HasKeyboardFocus();
    }
    
    return false;
}

FReply UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
    // 기본 클래스 구현 호출
    FReply Reply = SCompoundWidget::OnFocusReceived(MyGeometry, InFocusEvent);
    
    // 에디터블 텍스트로 포커스 전달
    if (EditableText.IsValid())
    {
        FSlateApplication::Get().SetKeyboardFocus(EditableText, EFocusCause::SetDirectly);
    }
    
    // 포커스 변경 처리
    HandleTextFocusChanged(true);
    
    return Reply;
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::OnFocusLost(const FFocusEvent& InFocusEvent)
{
    // 기본 클래스 구현 호출
    SCompoundWidget::OnFocusLost(InFocusEvent);
    
    // 포커스 변경 처리
    HandleTextFocusChanged(false);
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::HandleTextFocusChanged(bool bHasFocus)
{
    // 포커스가 변경되면 어떤 스크롤러가 보이는지 업데이트해야 할 수 있음
    if (bHasFocus)
    {
        // 텍스트가 포커스를 얻으면 스크롤링 중지
        if (TextScroller.IsValid())
        {
            TextScroller->SuspendScrolling();
            ScrollState = EScrollState::Idle;
        }
    }
    else
    {
        // 포커스가 사라지면 필요한 경우 스크롤링 재개
        if (TextScroller.IsValid() && !GetText().IsEmpty() && ScrollState != EScrollState::Idle)
        {
            TextScroller->ResetScrollState();
            TextScroller->StartScrolling();
            ScrollState = EScrollState::Scrolling;
        }
        
        if (HintTextScroller.IsValid() && GetText().IsEmpty())
        {
            HintTextScroller->ResetScrollState();
            HintTextScroller->StartScrolling();
        }
    }
}

//////////////////////////////////////////////////
// UPGMarqueeEditableTextBox
//////////////////////////////////////////////////

UPGMarqueeEditableTextBox::UPGMarqueeEditableTextBox()
{
}

// CommonTextScrollStyle에서 스크롤 옵션을 생성하는 헬퍼 함수
FTextScrollerOptions UPGMarqueeEditableTextBox::CreateScrollOptionsFromStyle(const UCommonTextScrollStyle* StyleCDO) const
{
    FTextScrollerOptions Options;
    
    if (StyleCDO)
    {
        // 스타일이 존재하는 경우 기본 스크롤 옵션 설정
        Options.Speed = 25.0f;       // 기본 스크롤 속도
        Options.StartDelay = 0.5f;    // 시작 지연 시간
        Options.EndDelay = 0.5f;      // 종료 지연 시간
        Options.FadeInDelay = 0.25f;  // 페이드 인 시간
        Options.FadeOutDelay = 0.25f; // 페이드 아웃 시간
    }
    else
    {
        // 스타일이 없는 경우 스크롤링 비활성화 (속도 0)
        Options.Speed = 0.0f;
        Options.StartDelay = 0.5f;
        Options.EndDelay = 0.5f;
        Options.FadeInDelay = 0.25f;
        Options.FadeOutDelay = 0.25f;
    }
    
    return Options;
}

// 힌트 텍스트용 스크롤 옵션을 생성하는 헬퍼 함수
FTextScrollerOptions UPGMarqueeEditableTextBox::CreateHintTextScrollOptionsFromStyle(const UCommonTextScrollStyle* StyleCDO) const
{
    FTextScrollerOptions Options;
    
    if (StyleCDO)
    {
        // 스타일이 존재하는 경우 힌트 텍스트 스크롤 옵션 설정
        Options.Speed = 15.0f;        // 힌트 텍스트는 일반 텍스트보다 느림
        Options.StartDelay = 1.0f;     // 시작 지연 시간
        Options.EndDelay = 1.0f;       // 종료 지연 시간
        Options.FadeInDelay = 0.25f;   // 페이드 인 시간
        Options.FadeOutDelay = 0.25f;  // 페이드 아웃 시간
    }
    else
    {
        // 스타일이 없는 경우 스크롤링 비활성화 (속도 0)
        Options.Speed = 0.0f;
        Options.StartDelay = 1.0f;
        Options.EndDelay = 1.0f;
        Options.FadeInDelay = 0.25f;
        Options.FadeOutDelay = 0.25f;
    }
    
    return Options;
}

TSharedRef<SWidget> UPGMarqueeEditableTextBox::RebuildWidget()
{
    // 스크롤 옵션 생성
    const UCommonTextScrollStyle* TextStyleCDO = GetScrollStyleCDO();
    const UCommonTextScrollStyle* HintTextStyleCDO = GetHintTextScrollStyleCDO();
    
    FTextScrollerOptions TextScrollOptions = CreateScrollOptionsFromStyle(TextStyleCDO);
    FTextScrollerOptions HintTextScrollOptions = CreateHintTextScrollOptionsFromStyle(HintTextStyleCDO);
    
    // 우리의 커스텀 slate 위젯 생성
    MarqueeEditableTextBox = SNew(SPGMarqueeEditableTextBox)
        .ScrollStyle(&WidgetStyle)
        .Text(GetText())
        .HintText(GetHintText())
        .Font(GetFont())
        .ColorAndOpacity(GetColorAndOpacity())
        .IsReadOnly(GetIsReadOnly())
        .IsPassword(GetIsPassword())
        .IsCaretMovedWhenGainFocus(GetIsCaretMovedWhenGainFocus())
        .SelectAllTextWhenFocused(GetSelectAllTextWhenFocused())
        .RevertTextOnEscape(GetRevertTextOnEscape())
        .ClearKeyboardFocusOnCommit(GetClearKeyboardFocusOnCommit())
        .SelectAllTextOnCommit(GetSelectAllTextOnCommit())
        .AllowContextMenu(AllowContextMenu)
        .MinDesiredWidth(GetMinimumDesiredWidth())
        .TextMargin(TextMargin)
        .TextScrollOptions(&TextScrollOptions)
        .HintTextScrollOptions(&HintTextScrollOptions)
        .OnTextChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnTextChanged))
        .OnTextCommitted(BIND_UOBJECT_DELEGATE(FOnTextCommitted, HandleOnTextCommitted));
    
    // 부모 클래스와의 호환성을 위해
    MyEditableTextBlock = StaticCastSharedRef<SEditableTextBox>(
        SNew(SEditableTextBox)
        .Style(&WidgetStyle)
        .Text(GetText())
    );
    
    return MarqueeEditableTextBox.ToSharedRef();
}

void UPGMarqueeEditableTextBox::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    
    if (MarqueeEditableTextBox.IsValid())
    {
        MarqueeEditableTextBox->SetText(GetText());
        MarqueeEditableTextBox->SetFont(GetFont());
        MarqueeEditableTextBox->SetColorAndOpacity(GetColorAndOpacity());
        
        // 스크롤 스타일 존재 여부에 따라 스크롤링 설정
        const bool bTextScrollEnabled = (ScrollStyle != nullptr);
        const bool bHintTextScrollEnabled = (HintTextScrollStyle != nullptr);
        
        MarqueeEditableTextBox->SetScrollingEnabled(bTextScrollEnabled);
        MarqueeEditableTextBox->SetHintTextScrollingEnabled(bHintTextScrollEnabled);
    }
}

void UPGMarqueeEditableTextBox::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    
    MarqueeEditableTextBox.Reset();
}

void UPGMarqueeEditableTextBox::HandleOnTextChanged(const FText& InText)
{
    // 텍스트 변경 이벤트를 위임하기 위한 부모 구현 호출
    Super::HandleOnTextChanged(InText);
    
    // 사용자가 입력하는 동안 스크롤링 중지
    if (MarqueeEditableTextBox.IsValid())
    {
        MarqueeEditableTextBox->SetScrollingEnabled(false);
    }
}

void UPGMarqueeEditableTextBox::HandleOnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
    // 커밋 이벤트를 위임하기 위한 부모 구현 호출
    Super::HandleOnTextCommitted(InText, CommitMethod);
    
    // 텍스트가 커밋된 후 스크롤링을 재개해야 하는지 확인
    if (MarqueeEditableTextBox.IsValid() && ScrollStyle != nullptr)
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float DeltaTime) 
        {
            if (MarqueeEditableTextBox.IsValid())
            {
                MarqueeEditableTextBox->ResetScrollState();
                const bool bEnabled = (ScrollStyle != nullptr);
                MarqueeEditableTextBox->SetScrollingEnabled(bEnabled);
            }
            return false; // 일회성 실행
        }), 0.1f); // 레이아웃이 완료되었는지 확인하기 위한 약간의 지연
    }
}

FSlateFontInfo UPGMarqueeEditableTextBox::GetFont() const
{
    return WidgetStyle.TextStyle.Font;
}

FSlateColor UPGMarqueeEditableTextBox::GetColorAndOpacity() const
{
    return WidgetStyle.TextStyle.ColorAndOpacity;
}

void UPGMarqueeEditableTextBox::SetScrollStyle(TSubclassOf<UCommonTextScrollStyle> InScrollStyle)
{
    // 스크롤 스타일 설정
    ScrollStyle = InScrollStyle;
    
    // 위젯이 유효한 경우 업데이트
    if (MarqueeEditableTextBox.IsValid())
    {
        // 스크롤 스타일이 설정되어 있는지 여부에 따라 스크롤링 활성화/비활성화
        const bool bEnabled = (ScrollStyle != nullptr);
        MarqueeEditableTextBox->SetScrollingEnabled(bEnabled);
    }
}

void UPGMarqueeEditableTextBox::SetHintTextScrollStyle(TSubclassOf<UCommonTextScrollStyle> InHintTextScrollStyle)
{
    // 힌트 텍스트 스크롤 스타일 설정
    HintTextScrollStyle = InHintTextScrollStyle;
    
    // 위젯이 유효한 경우 업데이트
    if (MarqueeEditableTextBox.IsValid())
    {
        // 힌트 텍스트 스크롤 스타일이 설정되어 있는지 여부에 따라 스크롤링 활성화/비활성화
        const bool bEnabled = (HintTextScrollStyle != nullptr);
        MarqueeEditableTextBox->SetHintTextScrollingEnabled(bEnabled);
    }
}

void UPGMarqueeEditableTextBox::ResetScrollState()
{
    if (MarqueeEditableTextBox.IsValid())
    {
        MarqueeEditableTextBox->ResetScrollState();
    }
}

void UPGMarqueeEditableTextBox::PauseScrolling()
{
    if (MarqueeEditableTextBox.IsValid())
    {
        MarqueeEditableTextBox->PauseScrolling();
    }
}

void UPGMarqueeEditableTextBox::ResumeScrolling()
{
    if (MarqueeEditableTextBox.IsValid())
    {
        MarqueeEditableTextBox->ResumeScrolling();
    }
}

const UCommonTextScrollStyle* UPGMarqueeEditableTextBox::GetScrollStyleCDO() const
{
    return ScrollStyle ? Cast<UCommonTextScrollStyle>(ScrollStyle->ClassDefaultObject) : nullptr;
}

const UCommonTextScrollStyle* UPGMarqueeEditableTextBox::GetHintTextScrollStyleCDO() const
{
    // HintTextScrollStyle이 설정된 경우에만 해당 스타일 사용
    if (HintTextScrollStyle)
    {
        return Cast<UCommonTextScrollStyle>(HintTextScrollStyle->ClassDefaultObject);
    }
    
    // HintTextScrollStyle이 설정되지 않은 경우 null 반환
    return nullptr;
}