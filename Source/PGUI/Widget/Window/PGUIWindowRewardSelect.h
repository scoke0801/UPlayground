#pragma once
#include "CoreMinimal.h"
#include "PGUIWindow.h"
#include "PGData/DataTable/Stage/PGStageDataRow.h"
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
    void SetStatus(const FText& Text) { StatusText = Text; bIsStatus = true; }
    void SetRewardId(int StageId);
    void SetChoices(FGuid InToken, const TArray<FPGStageReward>& InChoices);
    UFUNCTION(BlueprintCallable, Category="PG|Reward")
    bool SubmitChoice(int32 Index);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    friend class UPGCheatManager;
    FGuid Token;
    bool bIsStatus = false;
    FText StatusText;
    UPROPERTY(Transient)
    TArray<FPGStageReward> Choices;
    UPROPERTY(Transient) TArray<TObjectPtr<class UPGUIRewardCard>> Cards;
    UPROPERTY(Transient) TObjectPtr<class UTextBlock> BuildCountdown;
    float PresentationTime = 0.f;
    float ConfirmTime = 0.f;
    bool bConfirming = false;
    int32 PendingIndex = INDEX_NONE;
    void BeginChoice(int32 Index);
    UFUNCTION() void SelectFirst();
    UFUNCTION() void SelectSecond();
    UFUNCTION() void SelectThird();
};
