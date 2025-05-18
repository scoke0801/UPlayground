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

//////////////////////////////////////////////////
// SPGMarqueeEditableTextBox
//////////////////////////////////////////////////

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::Construct(const FArguments& InArgs)
{
    bIsScrollingEnabled = true;
    
    // Get a pointer to the editable text style from CoreStyle
    const FEditableTextStyle* EditableTextStyle = &FCoreStyle::Get().GetWidgetStyle<FEditableTextStyle>("NormalEditableText");
    
    // Create the editable text widget with a custom style
    EditableText = SNew(SEditableText)
        .Text(InArgs._Text)
        .HintText(InArgs._HintText)
        .Style(EditableTextStyle)  // Pass as pointer
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
    
    // Create scroll options for the text scroller
    FTextScrollerOptions ScrollOptions;
    ScrollOptions.Speed = InArgs._ScrollSpeed;
    ScrollOptions.StartDelay = InArgs._StartDelay;
    ScrollOptions.EndDelay = InArgs._EndDelay;
    ScrollOptions.FadeInDelay = 0.0f;
    ScrollOptions.FadeOutDelay = 0.0f;
    
    // Create the text scroller that wraps just the editable text
    TextScroller = SNew(STextScroller)
        .ScrollOptions(ScrollOptions)
        [
            EditableText.ToSharedRef()
        ];
    
    // Create the border that will contain the scroller
    Border = SNew(SBorder)
        .BorderImage(&InArgs._Style->BackgroundImageNormal)
        .Padding(InArgs._TextMargin)
        [
            TextScroller.ToSharedRef()
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
        if (bIsScrollingEnabled)
        {
            TextScroller->StartScrolling();
        }
        else
        {
            TextScroller->SuspendScrolling();
        }
    }
}

void UPGMarqueeEditableTextBox::SPGMarqueeEditableTextBox::ResetScrollState()
{
    if (TextScroller.IsValid())
    {
        TextScroller->ResetScrollState();
        
        if (bIsScrollingEnabled)
        {
            TextScroller->StartScrolling();
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
        .Style(&WidgetStyle)
        .Text(GetText())
        .HintText(GetHintText())
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
        .OnTextChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnTextChanged))
        .OnTextCommitted(BIND_UOBJECT_DELEGATE(FOnTextCommitted, HandleOnTextCommitted));
    
    // Cache the text scroller reference for later use
    if (MarqueeEditableTextBox.IsValid())
    {
        SetScrollingEnabled(bIsScrollingEnabled);
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
        MarqueeEditableTextBox->SetScrollingEnabled(bIsScrollingEnabled);
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

void UPGMarqueeEditableTextBox::SetScrollingEnabled(bool bInIsScrollingEnabled)
{
    bIsScrollingEnabled = bInIsScrollingEnabled;
    
    if (MarqueeEditableTextBox.IsValid())
    {
        MarqueeEditableTextBox->SetScrollingEnabled(bIsScrollingEnabled);
    }
}

void UPGMarqueeEditableTextBox::ResetScrollState()
{
    if (MarqueeEditableTextBox.IsValid())
    {
        MarqueeEditableTextBox->ResetScrollState();
    }
}

const UCommonTextScrollStyle* UPGMarqueeEditableTextBox::GetScrollStyleCDO() const
{
    return ScrollStyle ? Cast<UCommonTextScrollStyle>(ScrollStyle->ClassDefaultObject) : nullptr;
}
