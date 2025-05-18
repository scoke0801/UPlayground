// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/EditableTextBox.h"
#include "CommonTextBlock.h"
#include "PGMarqueeEditableTextBox.generated.h"

class STextScroller;
struct FTextScrollerOptions;

/**
 * 마퀴(흐르는 텍스트) 스크롤 기능을 가진 EditableTextBox입니다.
 * - 텍스트만 자동으로 스크롤됩니다 (배경이나 테두리는 스크롤되지 않음)
 * - 사용자가 입력을 시작하면 스크롤이 중지됩니다
 * - 사용자가 입력을 마치고 텍스트가 사용 가능한 공간보다 길면 스크롤이 재개됩니다
 * - 일반 텍스트와 힌트 텍스트에 대해 서로 다른 스크롤 스타일을 지원합니다
 * - CommonTextBlock과 유사한 스크롤 상태 관리를 구현합니다
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
	 * 텍스트 스크롤링 활성화 여부를 설정합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Marquee Text")
	void SetScrollingEnabled(bool bInIsScrollingEnabled);
	
	/**
	 * 힌트 텍스트 스크롤링 활성화 여부를 설정합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Marquee Text")
	void SetHintTextScrollingEnabled(bool bInIsHintTextScrollingEnabled);

	/**
	 * 스크롤링 상태를 처음으로 재설정합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Marquee Text")
	void ResetScrollState();

	/**
	 * 스크롤링을 비활성화하지 않고 일시적으로 중지합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Marquee Text")
	void PauseScrolling();

	/**
	 * 일시 중지된 스크롤링을 재개합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Marquee Text")
	void ResumeScrolling();

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
	
	/** 텍스트 변경 시 호출되는 콜백 */
	virtual void HandleOnTextChanged(const FText& InText) override;
	
	/** 텍스트가 커밋될 때 호출되는 콜백 */
	virtual void HandleOnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod) override;
	
	UFUNCTION(BlueprintCallable, Category="Appearance")
	FSlateFontInfo GetFont() const;
	
private:
	/** 일반 텍스트에 사용할 스크롤 스타일 에셋을 참조합니다. null로 설정하면 스크롤링이 비활성화됩니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	TSubclassOf<UCommonTextScrollStyle> ScrollStyle;

	/** 힌트 텍스트에 사용할 스크롤 스타일 에셋을 참조합니다. null로 설정하면 일반 ScrollStyle을 사용합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	TSubclassOf<UCommonTextScrollStyle> HintTextScrollStyle;

	/** 일반 텍스트의 초당 스크롤 속도(Slate 단위) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float ScrollSpeed = 25.0f;

	/** 힌트 텍스트의 초당 스크롤 속도(Slate 단위) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float HintTextScrollSpeed = 15.0f;

	/** 텍스트 스크롤을 시작하기 전 일시 중지 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float StartDelay = 0.5f;

	/** 텍스트 스크롤 후 일시 중지 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float EndDelay = 0.5f;

	/** 힌트 텍스트 스크롤을 시작하기 전 일시 중지 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float HintTextStartDelay = 1.0f;

	/** 힌트 텍스트 스크롤 후 일시 중지 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	float HintTextEndDelay = 1.0f;

	/** 초기에 스크롤링 활성화/비활성화 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	bool bIsScrollingEnabled = true;

	/** 초기에 힌트 텍스트 스크롤링 활성화/비활성화 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	bool bIsHintTextScrollingEnabled = true;
	
	/** 텍스트와 박스 가장자리 사이의 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marquee Text", meta = (AllowPrivateAccess = true))
	FMargin TextMargin = FMargin(5.0f);

	/** Slate 텍스트 스크롤러 위젯 (텍스트 내용만을 위한) */
	TSharedPtr<STextScroller> TextScroller;
	
	/** 스타일 업데이트를 위한 내부 EditableText 위젯 참조 */
	TSharedPtr<SEditableText> EditableText;

	/** 일반 텍스트의 스크롤 스타일 CDO 가져오기 */
	const UCommonTextScrollStyle* GetScrollStyleCDO() const;
	
	/** 힌트 텍스트의 스크롤 스타일 CDO 가져오기 */
	const UCommonTextScrollStyle* GetHintTextScrollStyleCDO() const;
	
	/** 스크롤링 상태 */
	enum class EScrollState
	{
		Idle,
		Scrolling,
		Paused
	};
	
	/** 마퀴 에디터블 텍스트 박스를 위한 커스텀 Slate 구현 */
	class SPGMarqueeEditableTextBox : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPGMarqueeEditableTextBox) {}
			SLATE_ATTRIBUTE(FText, Text)
			SLATE_ATTRIBUTE(FText, HintText)
			SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )
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
		
		/** 폰트 스타일 설정 */
		void SetFont(const FSlateFontInfo& InFont);
		
		/** EditableTextBox로 메서드 전달 */
		void SetText(const TAttribute<FText>& InText);
		FText GetText() const;
		FVector2D GetDesiredSize() const;
		
		/** 스크롤링 활성화 여부 설정 */
		void SetScrollingEnabled(bool bEnabled);
		
		/** 힌트 텍스트 스크롤링 활성화 여부 설정 */
		void SetHintTextScrollingEnabled(bool bEnabled);
		
		/** 스크롤링 상태 재설정 */
		void ResetScrollState();
		
		/** 스크롤링 일시 중지 */
		void PauseScrolling();
		
		/** 일시 중지된 경우 스크롤링 재개 */
		void ResumeScrolling();
		
		/** 입력이 현재 활성화되어 있는지 확인 */
		bool IsInputActive() const;
		
		/** 텍스트 편집 포커스가 변경될 때 호출됨 */
		virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
		virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
		
		/** 편집 가능한 텍스트 위젯 (실제 텍스트 입력) - 스타일 업데이트를 위해 public */
		TSharedPtr<SEditableText> EditableText;

	private:
		/** 스타일링 및 배경을 위한 테두리 위젯 */
		TSharedPtr<SBorder> Border;
		
		/** 마퀴 효과를 위한 텍스트 스크롤러 */
		TSharedPtr<STextScroller> TextScroller;
		
		/** 힌트 텍스트를 위한 텍스트 스크롤러 */
		TSharedPtr<STextScroller> HintTextScroller;
		
		/** 현재 스크롤링이 활성화되어 있는지 여부 */
		bool bIsScrollingEnabled = true;
		
		/** 현재 힌트 텍스트 스크롤링이 활성화되어 있는지 여부 */
		bool bIsHintTextScrollingEnabled = true;
		
		/** 현재 스크롤 상태 */
		EScrollState ScrollState = EScrollState::Idle;
		
		/** 실제 텍스트 또는 힌트 텍스트를 표시해야 하는지 결정 */
		bool ShouldShowHintText() const;
		
		/** 텍스트 포커스 변경 처리 */
		void HandleTextFocusChanged(bool bHasFocus);
	};
	
	/** 우리의 커스텀 slate 위젯 인스턴스 */
	TSharedPtr<SPGMarqueeEditableTextBox> MarqueeEditableTextBox;
};
