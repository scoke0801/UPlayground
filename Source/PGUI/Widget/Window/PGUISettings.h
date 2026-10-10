#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGUISettings.generated.h"

UCLASS()
class PGUI_API UPGUISettings : public UUserWidget
{
    GENERATED_BODY()
public:
    UPGUISettings(const FObjectInitializer& Initializer);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual void NativeDestruct() override;
};
