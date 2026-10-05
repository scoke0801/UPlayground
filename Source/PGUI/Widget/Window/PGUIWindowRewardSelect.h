#pragma once
#include "CoreMinimal.h"
#include "PGUIWindow.h"
#include "PGData/DataTable/Stage/PGStageDataRow.h"
#include "PGShared/Shared/Structure/PGRunResultView.h"
#include "Styling/SlateBrush.h"
#include "PGUIWindowRewardSelect.generated.h"

DECLARE_DELEGATE_RetVal_TwoParams(bool, FPGSubmitReward, FGuid, int32);

UCLASS()
class PGUI_API UPGUIWindowRewardSelect : public UPGUIWindow
{
    GENERATED_BODY()
public:
    FPGSubmitReward OnSubmit;
    FSimpleDelegate OnRetry;
    TWeakObjectPtr<class APGStageManager> StageOwner;
    void SetStatus(const FText& Text, const FText& Action = FText()) { StatusText = Text; StatusAction = Action; bIsStatus = true; bIsResult = false; }
    void SetResult(const FPGRunResultView& View) { Result = View; bIsStatus = bIsResult = true; }
    void SetRewardId(int StageId);
    void SetChoices(FGuid InToken, const TArray<FPGStageReward>& InChoices);
    UFUNCTION(BlueprintCallable, Category="PG|Reward") bool SubmitChoice(int32 Index);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    friend class UPGCheatManager;
    friend class APGPlayerController;
    FGuid Token;
    bool bIsStatus = false;
    bool bIsResult = false;
    bool bSubmitting = false;
    FText StatusText, StatusAction, Feedback;
    FPGRunResultView Result;
    TSharedPtr<class SBox> Frame;
    TSharedPtr<class SBorder> ResultBody;
    FSlateBrush LootBrush;
    FSlateBrush FrameBrush;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> FrameTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> LootIcon;
    UPROPERTY(Transient) TArray<FPGStageReward> Choices;
    UPROPERTY(Transient) TArray<TObjectPtr<class UPGUIRewardCard>> Cards;
    float PresentationTime = 0.f;
    float ConfirmTime = 0.f;
    bool bConfirming = false;
    int32 PendingIndex = INDEX_NONE;
    void BeginChoice(int32 Index);
    void RetryRun();
    TSharedRef<SWidget> MakeChoices();
    TSharedRef<SWidget> MakeResult();
    TSharedRef<SWidget> MakeStatus();
    FText CountdownText() const;
};
