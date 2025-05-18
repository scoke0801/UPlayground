// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/EditableTextBox.h"
#include "CommonTextBlock.h"
#include "PGMarqueeEditableTextBox.generated.h"

class STextScroller;
struct FTextScrollerOptions;

/**
 * EditableTextBox with marquee text scrolling capabilities.
 * - Automatically scrolls only the text (not the background or border)
 * - Stops scrolling when user starts typing
 * - Resumes scrolling when user finishes input and text is longer than available space
 */
UCLASS()
class UPLAYGROUND_API UPGMarqueeEditableTextBox : public UEditableTextBox
{
	GENERATED_BODY()
	
public:
	UPGMarqueeEditableTextBox();

	// UWidget interface
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UWidget interface

	/** 
	 * Sets whether text scrolling is enabled
	 */
	UFUNCTION(BlueprintCallable, Category = "Marquee Text")
	void SetScrollingEnabled(bool bInIsScrollingEnabled);

	/**
	 * Resets the scrolling state to the beginning
	 */
	UFUNCTION(BlueprintCallable, Category = "Marquee Text")
	void ResetScrollState();

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
	
	/** Callback for when text is changed */
	virtual void HandleOnTextChanged(const FText& InText) override;
	
	/** Callback for when text is committed */
	virtual void HandleOnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod) override;

private:
	/** References the scroll style asset to use. Setting to null disables scrolling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	TSubclassOf<UCommonTextScrollStyle> ScrollStyle;

	/** Speed (in Slate units) to scroll per-second */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float ScrollSpeed = 25.0f;

	/** Delay (in seconds) to pause before scrolling the text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float StartDelay = 0.5f;

	/** Delay (in seconds) to pause after scrolling the text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float EndDelay = 0.5f;

	/** If scrolling is enabled/disabled initially */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	bool bIsScrollingEnabled = true;
	
	/** Margin between the text and the box edge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	FMargin TextMargin = FMargin(5.0f);

	/** Slate text scroller widget (for the text content only) */
	TSharedPtr<STextScroller> TextScroller;

	/** Get the scroll style CDO */
	const UCommonTextScrollStyle* GetScrollStyleCDO() const;
	
	/** Custom Slate implementation for our marquee editable text box */
	class SPGMarqueeEditableTextBox : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPGMarqueeEditableTextBox) {}
			SLATE_ATTRIBUTE(FText, Text)
			SLATE_ATTRIBUTE(FText, HintText)
			SLATE_ARGUMENT(const FEditableTextBoxStyle*, Style)
			SLATE_ATTRIBUTE(bool, IsReadOnly)
			SLATE_ATTRIBUTE(bool, IsPassword)
			SLATE_ATTRIBUTE(float, MinDesiredWidth)
			SLATE_ATTRIBUTE(bool, IsCaretMovedWhenGainFocus)
			SLATE_ATTRIBUTE(bool, SelectAllTextWhenFocused)
			SLATE_ATTRIBUTE(bool, RevertTextOnEscape)
			SLATE_ATTRIBUTE(bool, ClearKeyboardFocusOnCommit)
			SLATE_ATTRIBUTE(bool, SelectAllTextOnCommit)
			SLATE_ATTRIBUTE(ETextJustify::Type, Justification)
			SLATE_ATTRIBUTE(bool, AllowContextMenu)
			SLATE_ATTRIBUTE(FMargin, TextMargin)
			SLATE_EVENT(FOnTextChanged, OnTextChanged)
			SLATE_EVENT(FOnTextCommitted, OnTextCommitted)
			SLATE_ARGUMENT(float, ScrollSpeed)
			SLATE_ARGUMENT(float, StartDelay)
			SLATE_ARGUMENT(float, EndDelay)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		
		/** Pass-through methods to EditableTextBox */
		void SetText(const TAttribute<FText>& InText);
		FText GetText() const;
		FVector2D GetDesiredSize() const;
		
		/** Set whether scrolling is enabled */
		void SetScrollingEnabled(bool bEnabled);
		
		/** Reset the scrolling state */
		void ResetScrollState();

	private:
		/** Border widget for styling and background */
		TSharedPtr<SBorder> Border;
		
		/** The editable text widget (actual text input) */
		TSharedPtr<SEditableText> EditableText;
		
		/** Text scroller for marquee effect */
		TSharedPtr<STextScroller> TextScroller;
		
		/** Whether scrolling is currently enabled */
		bool bIsScrollingEnabled = true;
	};
	
	/** Our custom slate widget instance */
	TSharedPtr<SPGMarqueeEditableTextBox> MarqueeEditableTextBox;
};
