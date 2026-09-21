#pragma once
#include "CoreMinimal.h"
#include "PGUI/Widget/Base/PGWidgetBase.h"
#include "PGShared/Shared/Enum/PGRewardTypes.h"
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
    void SetConfirmed(bool bSelected);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY(Transient) TObjectPtr<class UBorder> Frame;
    UPROPERTY(Transient) TObjectPtr<class UButton> Button;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Icon;
    FText Title, Description;
    FLinearColor GradeColor = FLinearColor::White;
    int32 Index = 0;
    int32 IconPanel = -1;
    UFUNCTION() void Clicked();
    UFUNCTION() void Hovered();
    UFUNCTION() void Unhovered();
};
