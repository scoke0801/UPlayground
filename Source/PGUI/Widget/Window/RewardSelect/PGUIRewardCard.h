#pragma once
#include "CoreMinimal.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGShared/Shared/Enum/PGRewardTypes.h"
#include "Styling/SlateBrush.h"
#include "PGUIRewardCard.generated.h"

DECLARE_DELEGATE_OneParam(FPGRewardCardSelected, int32);
UCLASS()
class PGUI_API UPGUIRewardCard : public UPGWidgetBase
{
    GENERATED_BODY()
public:
    FPGRewardCardSelected OnSelected;
    void Configure(int32 InIndex, FText InTitle, FText InDescription, EPGRewardGrade InGrade, UTexture2D* InIcon, int32 InIconPanel = -1);
    void SetGrade(EPGRewardGrade InGrade);
    void SetBuildContext(const FText& Text) { Context = Text; }
    void SetContinueOnly(bool bValue) { bContinueOnly = bValue; }
    void SetConfirmed(bool bSelected);
    void ResetConfirmation();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Icon;
    FSlateBrush IconBrush;
    FText Title, Description, Context;
    FLinearColor GradeColor = FLinearColor::White;
    int32 Index = 0;
    int32 IconPanel = -1;
    bool bConfirmed = false;
    bool bContinueOnly = false;
};
