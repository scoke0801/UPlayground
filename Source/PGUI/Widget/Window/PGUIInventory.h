#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGUIInventory.generated.h"
UCLASS()
class PGUI_API UPGUIInventory : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    TSharedPtr<class SVerticalBox> Rows;
    TSharedPtr<class STextBlock> StatusText;
    int32 MinRarity = 0;
    FGuid PendingDiscard;
    bool bConfirmRecovery = false;
    void Refresh();
};
