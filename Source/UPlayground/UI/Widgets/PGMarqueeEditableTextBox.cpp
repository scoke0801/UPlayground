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
    bIsScrollingEnabled = true;
    bIsHintTextScrollingEnabled = true;
    ScrollState = EScrollState::Idle;
    
    // Get style from the parent EditableTextBox - no need to force a specific style
    // Create the editable text widget using the default styling
    EditableText = SNew(SEditableText)
        .Text(InArgs._Text)
        .HintText(InArgs._HintText)
        .Font(InArgs._Font)  // 폰트 속성 적용
        .IsReadOnly(InArgs._IsReadOnly)
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
    
    // Create scroll options for the normal text
    FTextScrollerOptions TextScrollOptions;
    TextScrollOptions.Speed = InArgs._ScrollSpeed;
    TextScrollOptions.StartDelay = InArgs._StartDelay;
    TextScrollOptions.EndDelay = InArgs._EndDelay;
    TextScrollOptions.FadeInDelay = 0.0f;
    TextScrollOptions.FadeOutDelay = 0.0f;
    
    // Create scroll options for the hint text
    FTextScrollerOptions HintTextScrollOptions;
    HintTextScrollOptions.Speed = InArgs._HintTextScrollSpeed;
    HintTextScrollOptions.StartDelay = InArgs._HintTextStartDelay;
    HintTextScrollOptions.EndDelay = InArgs._HintTextEndDelay;
    HintTextScrollOptions.FadeInDelay = 0.0f;
    HintTextScrollOptions.FadeOutDelay = 0.0f;
    
    // Create text block for hint text display
    TSharedRef<STextBlock> HintTextBlock = SNew(STextBlock)
        .Text(InArgs._HintText)
        // Use subdued foreground color for hint text - this will respect the system styling
        .ColorAndOpacity(FSlateColor::UseSubduedForeground());
    
    // Create the text scroller for normal text
    TextScroller = SNew(STextScroller)
        .ScrollOptions(TextScrollOptions)
        [
            EditableText.ToSharedRef()
        ];
    
    // Create the hint text scroller
    HintTextScroller = SNew(STextScroller)
        .ScrollOptions(HintTextScrollOptions)
        [
            HintTextBlock
        ];
    
    // Create a container for both scrollers
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
    
    // Create the border that will contain the scrollers
    Border = SNew(SBorder)
        .BorderImage(&InArgs._ScrollStyle->BackgroundImageNormal)
        .Padding(InArgs._TextMargin)
        [
            ContentWidget
        ];

    // Set the content to the border
    ChildSlot
    [
        Border.ToSharedRef()
    ];
    
    // Start scrolling if enabled
    if (bIsScrollingEnabled)
    {
        TextScroller->StartScrolling();
    }
    else
    {
        TextScroller->SuspendScrolling();
    }
    
    if (bIsHintTextScrollingEnabled)
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
    bIsScrollingEnabled = bEnabled;
    
    if (TextScroller.IsValid())
    {
        if (bIsScrollingEnabled && ScrollState != EScrollState::Paused)
        {
            TextScroller->StartScrolling();
            ScrollState = EScrollState::Scrolling;
        }
        else
        {
            TextScroller->SuspendScrolling();
            if (!bIsScrollingEnabled)
            {
                ScrollState = EScrollState::Idle;
            }
        }
    }
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::SetHintTextScrollingEnabled(bool bEnabled)
{
    bIsHintTextScrollingEnabled = bEnabled;
    
    if (HintTextScroller.IsValid())
    {
        if (bIsHintTextScrollingEnabled)
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
        
        if (bIsScrollingEnabled && ScrollState != EScrollState::Paused)
        {
            TextScroller->StartScrolling();
            ScrollState = EScrollState::Scrolling;
        }
    }
    
    if (HintTextScroller.IsValid())
    {
        HintTextScroller->ResetScrollState();
        
        if (bIsHintTextScrollingEnabled)
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
        if (TextScroller.IsValid() && bIsScrollingEnabled)
        {
            TextScroller->StartScrolling();
            ScrollState = EScrollState::Scrolling;
        }
        else
        {
            ScrollState = EScrollState::Idle;
        }
        
        if (HintTextScroller.IsValid() && bIsHintTextScrollingEnabled)
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
    // Call base class implementation
    FReply Reply = SCompoundWidget::OnFocusReceived(MyGeometry, InFocusEvent);
    
    // Forward focus to editable text
    if (EditableText.IsValid())
    {
        FSlateApplication::Get().SetKeyboardFocus(EditableText, EFocusCause::SetDirectly);
    }
    
    // Handle focus change
    HandleTextFocusChanged(true);
    
    return Reply;
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::OnFocusLost(const FFocusEvent& InFocusEvent)
{
    // Call base class implementation
    SCompoundWidget::OnFocusLost(InFocusEvent);
    
    // Handle focus change
    HandleTextFocusChanged(false);
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::HandleTextFocusChanged(bool bHasFocus)
{
    // When focus changes, we may need to update which scroller is visible
    if (bHasFocus)
    {
        // Suspend scrolling when text gets focus
        if (TextScroller.IsValid())
        {
            TextScroller->SuspendScrolling();
            ScrollState = EScrollState::Idle;
        }
    }
    else
    {
        // When focus is lost, resume scrolling if needed
        if (TextScroller.IsValid() && bIsScrollingEnabled && !GetText().IsEmpty())
        {
            TextScroller->ResetScrollState();
            TextScroller->StartScrolling();
            ScrollState = EScrollState::Scrolling;
        }
        
        if (HintTextScroller.IsValid() && bIsHintTextScrollingEnabled && GetText().IsEmpty())
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

TSharedRef<SWidget> UPGMarqueeEditableTextBox::RebuildWidget()
{
    // Create our custom slate widget
    MarqueeEditableTextBox = SNew(SPGMarqueeEditableTextBox)
        .ScrollStyle(&WidgetStyle)
        .Text(GetText())
        .HintText(GetHintText())
        .Font(GetFont())  // 폰트 속성 전달
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
        .ScrollSpeed(ScrollSpeed)
        .StartDelay(StartDelay)
        .EndDelay(EndDelay)
        .HintTextScrollSpeed(HintTextScrollSpeed)
        .HintTextStartDelay(HintTextStartDelay)
        .HintTextEndDelay(HintTextEndDelay)
        .OnTextChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnTextChanged))
        .OnTextCommitted(BIND_UOBJECT_DELEGATE(FOnTextCommitted, HandleOnTextCommitted));
    
    // Cache the text scroller reference for later use
    if (MarqueeEditableTextBox.IsValid())
    {
        SetScrollingEnabled(bIsScrollingEnabled);
        SetHintTextScrollingEnabled(bIsHintTextScrollingEnabled);
    }
    
    // For compatibility with parent class
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
        MarqueeEditableTextBox->SetFont(GetFont());  // 폰트 속성 동기화
        MarqueeEditableTextBox->SetScrollingEnabled(bIsScrollingEnabled);
        MarqueeEditableTextBox->SetHintTextScrollingEnabled(bIsHintTextScrollingEnabled);
    }
}

void UPGMarqueeEditableTextBox::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    
    MarqueeEditableTextBox.Reset();
}

void UPGMarqueeEditableTextBox::HandleOnTextChanged(const FText& InText)
{
    // Call the parent implementation to delegate the text change event
    Super::HandleOnTextChanged(InText);
    
    // Suspend scrolling while user is typing
    if (MarqueeEditableTextBox.IsValid())
    {
        MarqueeEditableTextBox->SetScrollingEnabled(false);
    }
}

void UPGMarqueeEditableTextBox::HandleOnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
    // Call the parent implementation to delegate the commit event
    Super::HandleOnTextCommitted(InText, CommitMethod);
    
    // After text is committed, check if we need to resume scrolling
    if (MarqueeEditableTextBox.IsValid() && bIsScrollingEnabled)
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float DeltaTime) 
        {
            if (MarqueeEditableTextBox.IsValid())
            {
                MarqueeEditableTextBox->ResetScrollState();
                MarqueeEditableTextBox->SetScrollingEnabled(bIsScrollingEnabled);
            }
            return false; // One-time execution
        }), 0.1f); // Slight delay to ensure layout is complete
    }
}

FSlateFontInfo UPGMarqueeEditableTextBox::GetFont() const
{
    return WidgetStyle.Font_DEPRECATED;
}

void UPGMarqueeEditableTextBox::SetScrollingEnabled(bool bInIsScrollingEnabled)
{
    bIsScrollingEnabled = bInIsScrollingEnabled;
    
    if (MarqueeEditableTextBox.IsValid())
    {
        MarqueeEditableTextBox->SetScrollingEnabled(bIsScrollingEnabled);
    }
}

void UPGMarqueeEditableTextBox::SetHintTextScrollingEnabled(bool bInIsHintTextScrollingEnabled)
{
    bIsHintTextScrollingEnabled = bInIsHintTextScrollingEnabled;
    
    if (MarqueeEditableTextBox.IsValid())
    {
        MarqueeEditableTextBox->SetHintTextScrollingEnabled(bIsHintTextScrollingEnabled);
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
    // Use the HintTextScrollStyle if set, otherwise fall back to normal ScrollStyle
    if (HintTextScrollStyle)
    {
        return Cast<UCommonTextScrollStyle>(HintTextScrollStyle->ClassDefaultObject);
    }
    
    return GetScrollStyleCDO();
}
