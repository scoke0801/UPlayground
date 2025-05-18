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
 * - Supports different scroll styles for normal text and hint text
 * - Implements scroll state management similar to CommonTextBlock
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
	 * Sets whether hint text scrolling is enabled
	 */
	UFUNCTION(BlueprintCallable, Category = "Marquee Text")
	void SetHintTextScrollingEnabled(bool bInIsHintTextScrollingEnabled);

	/**
	 * Resets the scrolling state to the beginning
	 */
	UFUNCTION(BlueprintCallable, Category = "Marquee Text")
	void ResetScrollState();

	/**
	 * Pauses scrolling temporarily without disabling it
	 */
	UFUNCTION(BlueprintCallable, Category = "Marquee Text")
	void PauseScrolling();

	/**
	 * Resumes scrolling if it was paused
	 */
	UFUNCTION(BlueprintCallable, Category = "Marquee Text")
	void ResumeScrolling();

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
	
	/** Callback for when text is changed */
	virtual void HandleOnTextChanged(const FText& InText) override;
	
	/** Callback for when text is committed */
	virtual void HandleOnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod) override;
	
	UFUNCTION(BlueprintCallable, Category="Appearance")
	FSlateFontInfo GetFont() const;
	
private:
	/** References the scroll style asset to use for normal text. Setting to null disables scrolling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	TSubclassOf<UCommonTextScrollStyle> ScrollStyle;

	/** References the scroll style asset to use for hint text. Setting to null uses the normal ScrollStyle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	TSubclassOf<UCommonTextScrollStyle> HintTextScrollStyle;

	/** Speed (in Slate units) to scroll per-second for normal text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float ScrollSpeed = 25.0f;

	/** Speed (in Slate units) to scroll per-second for hint text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float HintTextScrollSpeed = 15.0f;

	/** Delay (in seconds) to pause before scrolling the text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float StartDelay = 0.5f;

	/** Delay (in seconds) to pause after scrolling the text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float EndDelay = 0.5f;

	/** Delay (in seconds) to pause before scrolling the hint text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float HintTextStartDelay = 1.0f;

	/** Delay (in seconds) to pause after scrolling the hint text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float HintTextEndDelay = 1.0f;

	/** If scrolling is enabled/disabled initially */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	bool bIsScrollingEnabled = true;

	/** If hint text scrolling is enabled/disabled initially */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	bool bIsHintTextScrollingEnabled = true;
	
	/** Margin between the text and the box edge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	FMargin TextMargin = FMargin(5.0f);

	/** Slate text scroller widget (for the text content only) */
	TSharedPtr<STextScroller> TextScroller;
	
	/** Reference to the inner EditableText widget for style updates */
	TSharedPtr<SEditableText> EditableText;

	/** Get the scroll style CDO for normal text */
	const UCommonTextScrollStyle* GetScrollStyleCDO() const;
	
	/** Get the scroll style CDO for hint text */
	const UCommonTextScrollStyle* GetHintTextScrollStyleCDO() const;
	
	/** States for scrolling */
	enum class EScrollState
	{
		Idle,
		Scrolling,
		Paused
	};
	
	/** Custom Slate implementation for our marquee editable text box */
	class SPGMarqueeEditableTextBox : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPGMarqueeEditableTextBox) {}
			SLATE_ATTRIBUTE(FText, Text)
			SLATE_ATTRIBUTE(FText, HintText)
			SLATE_ARGUMENT(const FEditableTextBoxStyle*, ScrollStyle)
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
			SLATE_ATTRIBUTE(FSlateFontInfo, Font)  // 폰트 속성 추가
			SLATE_EVENT(FOnTextChanged, OnTextChanged)
			SLATE_EVENT(FOnTextCommitted, OnTextCommitted)
			SLATE_ARGUMENT(float, ScrollSpeed)
			SLATE_ARGUMENT(float, StartDelay)
			SLATE_ARGUMENT(float, EndDelay)
			SLATE_ARGUMENT(float, HintTextScrollSpeed)
			SLATE_ARGUMENT(float, HintTextStartDelay)
			SLATE_ARGUMENT(float, HintTextEndDelay)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		
		/** Set the font style */
		void SetFont(const FSlateFontInfo& InFont);
		
		/** Pass-through methods to EditableTextBox */
		void SetText(const TAttribute<FText>& InText);
		FText GetText() const;
		FVector2D GetDesiredSize() const;
		
		/** Set whether scrolling is enabled */
		void SetScrollingEnabled(bool bEnabled);
		
		/** Set whether hint text scrolling is enabled */
		void SetHintTextScrollingEnabled(bool bEnabled);
		
		/** Reset the scrolling state */
		void ResetScrollState();
		
		/** Pause scrolling temporarily */
		void PauseScrolling();
		
		/** Resume scrolling if paused */
		void ResumeScrolling();
		
		/** Check if input is currently active */
		bool IsInputActive() const;
		
		/** Called when text edit focus changes */
		virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
		virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
		
		/** The editable text widget (actual text input) - public for style updates */
		TSharedPtr<SEditableText> EditableText;

	private:
		/** Border widget for styling and background */
		TSharedPtr<SBorder> Border;
		
		/** Text scroller for marquee effect */
		TSharedPtr<STextScroller> TextScroller;
		
		/** Text scroller for hint text */
		TSharedPtr<STextScroller> HintTextScroller;
		
		/** Whether scrolling is currently enabled */
		bool bIsScrollingEnabled = true;
		
		/** Whether hint text scrolling is currently enabled */
		bool bIsHintTextScrollingEnabled = true;
		
		/** Current scroll state */
		EScrollState ScrollState = EScrollState::Idle;
		
		/** Determine if we should show real text or hint text */
		bool ShouldShowHintText() const;
		
		/** Handle changes in text focus */
		void HandleTextFocusChanged(bool bHasFocus);
	};
	
	/** Our custom slate widget instance */
	TSharedPtr<SPGMarqueeEditableTextBox> MarqueeEditableTextBox;
};
